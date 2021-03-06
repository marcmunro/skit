#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
m4_define([SKIT_VERSION],
   esyscmd([grep 'SKIT_VERSION ' src/skit.h | \
            cut -d\" -f2 | tr -d '\n']))

AC_INIT(skit, SKIT_VERSION, skit-general@pgfoundry.org)
AC_SUBST(MAJOR_VERSION, esyscmd([grep 'SKIT_VERSION ' src/skit.h | \
            cut -d\" -f2 | cut -d. -f1 | tr -d '\n']))
AC_SUBST(MINOR_VERSION, esyscmd([grep 'SKIT_VERSION ' src/skit.h | \
            cut -d\" -f2 | cut -d. -f2 | cut -d\  -f1 | tr -d '\n']))

AC_CONFIG_SRCDIR([test/check_filepath.c])

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([stdlib.h string.h unistd.h ])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([memmove memset regcomp strchr strcspn strpbrk])

AC_DEFUN(PKG_CHECK_MODULES, [
  succeeded=no

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get
pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then
        AC_MSG_CHECKING(for $2)

        if $PKG_CONFIG --exists "$2" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING($1_CFLAGS)
            $1_CFLAGS=`$PKG_CONFIG --cflags "$2"`
            AC_MSG_RESULT($$1_CFLAGS)

            AC_MSG_CHECKING($1_LIBS)
            $1_LIBS=`$PKG_CONFIG --libs "$2"`
            AC_MSG_RESULT($$1_LIBS)
        else
            $1_CFLAGS=""
            $1_LIBS=""
            AC_MSG_RESULT([no])
        fi

        AC_SUBST($1_CFLAGS)
        AC_SUBST($1_LIBS)
     else
        echo "*** Your version of pkg-config is too old. You need version
$PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig";
     fi
  fi

  if test $succeeded = yes; then
     ifelse([$3], , :, [$3])
  else
     ifelse([$4], , AC_MSG_ERROR([Library requirements ($2) not met; consider
adjusting the PKG_CONFIG_PATH environment variable if your libraries are in a
nonstandard prefix so pkg-config can find them.]), [$4])
  fi
])



AC_ARG_WITH(pgconfigdir,        [  --with-pgconfigdir=<dir>  Location of the PostgreSQL pg_config program. ])
AC_ARG_WITH(pgincludedir,       [  --with-pgincludedir=<dir> Location of the PostgreSQL headers. ])
AC_ARG_WITH(pglibdir,           [  --with-pglibdir=<dir>     Location of the PostgreSQL libs. ])


dnl Checks for libpq and libpq-fe.h, sets variables if found.
AC_DEFUN(POSTGRESQL_CHECKS, [
        AC_SUBST(PQINCPATH)
        AC_SUBST(PQLIB)

dnl Checking for pg_config in a list of possible locations.
PG_POSSIBLE_VERSION=[`psql --version | head -1 | sed 's/.*\([0-9][0-9]*\.[0-9][0-9]*\)\.[0-9][0-9]*.*/\1/'`]

AC_PATH_PROG([PG_CONFIG], 
	     [pg_configdir/pg_config${ac_exeext}], 
	     [`which pg_config${ac_exeext}`],
	     [/usr/lib/postgresql/$PG_POSSIBLE_VERSION/bin:/usr/local/pgsql/bin /usr/local/bin:/usrz/bin:/bin:/usr/lib/postgresql/bin:/opt/local/pgsql/bin:/opt/pgsql/bin/])


if test "x${with_pglibdir}" != "x" ; then
    dnl pglibdir was provided explicitly, so let's use it
    AC_MSG_CHECKING([PostgreSQL library from --with_pglibdir])
    PQLIB="-I${with_pglibdir}"
    AC_MSG_RESULT([${PQLIB}])
else
    dnl Check for PQLIB the simple way.
    AC_CHECK_LIB(pq,PQconnectdb,FOUND_PQLIB=1)
    if test $FOUND_PQLIB ; then
        PQLIB="-lpq"
    else
       AC_MSG_CHECKING([PostgreSQL library from pg_config])
       if test -x "${PG_CONFIG}" ; then
       	  dnl We have found pg_config so use that to figure out pkglibdir
    	  PQLIB=-I`${PG_CONFIG} --pkglibdir`
	  AC_MSG_RESULT([${PQLIB}])
	echo
	  AC_MSG_RESULT([no])
          echo PostgreSQL library not found.
          exit 0
	fi
    fi
fi


AC_MSG_CHECKING([PostgreSQL headers])
if test "x${with_pgincludedir}" != "x" ; then
    dnl pgincludedir was provided explicitly, so let's use it
    pg_inc_dir="${with_pgincludedir}"
    AC_MSG_RESULT([${pg_inc_dir}])
elif test -x "${PG_CONFIG}" ; then
    dnl We have found pg_config so use that to figure out includedir
    pg_inc_dir=`${PG_CONFIG} --pkgincludedir`
    AC_MSG_RESULT([${pg_inc_dir}])
else
    AC_MSG_RESULT([no])
    exit 0
fi

dnl Check for libpq-fe.h
AC_CHECK_HEADERS("$pg_inc_dir/libpq-fe.h")
if test $HAVE_HEADER-LIBPQ-FE.H ; then
    PQINCPATH="-I$pg_inc_dir"
else
    echo PostgreSQL header libpq-fe.h not found!
    exit 0
fi
])


POSTGRESQL_CHECKS
PKG_CHECK_MODULES(GLIB, glib-2.0)
PKG_CHECK_MODULES(XML, [libxml-2.0 >= 2.5])
PKG_CHECK_MODULES(XSLT, [libxslt >= 1.1])
PKG_CHECK_MODULES(EXSLT, [libexslt >= 0.8])
PKG_CHECK_MODULES(CHECK, [check >= 0.9.6])


AC_MSG_CHECKING(whether to enable debugging)
debug_default="yes"
AC_ARG_ENABLE(debug, [  --enable-debug=[no/yes] turn on debugging
                       [default=$debug_default]],, enable_debug=$debug_default)

dnl Yes, DEBUG symbol will be defined
if test "x$enable_debug" = "xyes"; then
        AC_SUBST(DEBUG, "yes")
	AC_MSG_RESULT(yes)
else
        AC_SUBST(DEBUG)
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(whether to enable c assertions)
cassert_default="no"
AC_ARG_ENABLE(cassert, [  --enable-cassert=[no/yes] turn on c assertions
                  [default=cassert_default]],, enable_cassert=$cassert_default)

dnl Yes, CASSERT symbol will be defined
if test "x$enable_cassert" = "xyes"; then
        AC_SUBST(CASSERT, "yes")
	AC_MSG_RESULT(yes)
else
        AC_SUBST(CASSERT)
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(whether to enable memory tracking)
mtrack_default="yes"
AC_ARG_ENABLE(mtrack, [  --enable-mtrack=[no/yes] turn on memory tracking
                       [default=$mtrack_default]],, enable_mtrack=$mtrack_default)

dnl Yes, MTRACK symbol will be defined
if test "x$enable_mtrack" = "xyes"; then
        AC_SUBST(MTRACK, "yes")
	AC_MSG_RESULT(yes)
else
        AC_SUBST(MTRACK)
	AC_MSG_RESULT(no)
fi


# Docbook
#
#PGAC_PROG_NSGMLS
#PGAC_PROG_JADE
#PGAC_CHECK_DOCBOOK(4.2)
#PGAC_PATH_DOCBOOK_STYLESHEETS
#PGAC_PATH_COLLATEINDEX
AC_CHECK_PROGS(XSLTPROC, xsltproc)

dnl Find docbook entities file


AC_DEFUN([SKIT_PATH_DOCBOOK_ENTITIES],
[AC_ARG_VAR(DOCBOOKENT, [location of Docbook entities])dnl
AC_MSG_CHECKING([for DocBook entities])
AC_CACHE_VAL([pgac_cv_path_entities],
[if test -n "$DOCBOOKENT"; then
  skit_path=$DOCBOOKENT
else
  for skit_prefix in /usr /usr/local /opt /sw; do
    for skit_infix in share lib; do
      for skit_postfix in \
        xml/docbook/schema/dtd/4.5 \
	sgml/docbook/dtd/4.5 \
	sgml/docbook/xml-dtd-4.5
      do
        skit_candidate=$skit_prefix/$skit_infix/$skit_postfix
        if test -r "$skit_candidate/dbcentx.mod" 
        then
          skit_path=$skit_candidate/dbcentx.mod
          break 3
        fi
      done
    done
  done
fi])
DOCBOOKENT=$skit_path
AC_SUBST([DOCBOOKENT])
if test -n "$DOCBOOKENT"; then
  AC_MSG_RESULT([$DOCBOOKENT])
else
  AC_MSG_RESULT(no)
fi])

SKIT_PATH_DOCBOOK_ENTITIES



AC_CONFIG_FILES([Makefile.global])
AC_OUTPUT

