# FILE:
#    configure.ac
#
# VERSION:
#  0.0.2, Mon Mar  5 10:54:38 PST 2012
#
# LICENSE:
#  This file is part of pxargs.
#
#  pxargs is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  pxargs is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with pxargs. If not, see http://www.gnu.org/licenses/.
#
#  Copyright (C) 2012 Andrew Michaelis

AC_INIT([pxargs], [0.0.3-beta])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE()

AC_CONFIG_MACRO_DIR([m4])

# Check for programs...
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_CC

# For the mpicc (if applicable)
AC_ARG_VAR([MPICC_PROG], [path to mpicc program])
AC_PATH_PROG([MPICC_PROG], [mpicc])
# Look for mpicc (if applicable)
if test -n "$MPICC_PROG"; then
   cc_save="$CC"
   CC="$MPICC_PROG"
   AC_SUBST(MPICC_PROG)
fi

# force some flags for gcc
if test "x$CC" = "xgcc"; then
   CFLAGS="$CFLAGS -Wall"
fi

# force some flags intel icc
if test "x$CC" = "xicc"; then
   CFLAGS="$CFLAGS -warn all"
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(void*)
   
AC_C_BIGENDIAN

# Check for headers
AC_CHECK_HEADERS([limits.h]) 
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([getopt.h]) 
AC_CHECK_HEADERS([sys/types.h]) 
AC_CHECK_HEADERS([sys/wait.h]) 
AC_CHECK_HEADERS([signal.h]) 
AC_HEADER_TIME

# Check for functions
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([getopt strncpy strtok_r])
AC_CHECK_FUNCS([getopt_long getenv])
AC_CHECK_FUNCS([sigaction])
AC_CHECK_FUNCS([setpgid])
AC_CHECK_FUNCS([alarm])
AC_CHECK_FUNCS([_exit])
AC_CHECK_LIB([z], [inflate])

AC_CHECK_HEADER([mpi.h],[have_mpi_h=yes],[have_mpi_h=no])
if test "x$have_mpi_h" = xno; then
   AC_MSG_ERROR([Can't find or mpi.h, Check CPPFLAGS MPICC etc...  ])
fi
AC_CHECK_LIB([mpi], [MPI_Init])

AC_DEFINE([USER_PATH_MAX], [2048], [If PATH_MAX missing, no limits.h, then use this])

# Data dir helper install path
CPPFLAGS="$CPPFLAGS "'-DDATADIR="\"$(datadir)\""'

AC_OUTPUT(src/Makefile man/Makefile Makefile) 

# post proc printout stuff
IF_ENABLED_DISABLED() {
   if test "x$2" = "xyes"; then
      echo "$1 Enabled"
   else
      echo "$1 Disabled"
   fi
}

echo ""
echo "                       Configure Summary"
echo "                       ================="
echo ""
echo "Compiling Options:"
echo "--------------------"
echo ""
echo "         C Compiler: $CC"
echo "             CFLAGS: $CFLAGS"
echo "           CPPFLAGS: $CPPFLAGS"
echo "            LDFLAGS: $LDFLAGS"
echo "               LIBS: $LIBS"
echo ""
echo "Platform Type Sizes:"
echo "--------------------"
echo ""
echo "      SIZEOF size_t: $ac_cv_sizeof_size_t"
echo "       SIZEOF off_t: $ac_cv_sizeof_off_t"
echo "         SIZEOF int: $ac_cv_sizeof_int"
echo "        SIZEOF char: $ac_cv_sizeof_char"
echo "        SIZEOF long: $ac_cv_sizeof_long"
echo "   SIZEOF long long: $ac_cv_sizeof_long_long"
echo "       SIZEOF voidp: $ac_cv_sizeof_voidp"
echo ""
echo "Miscellaneous:"
echo "--------------------"
echo "             PREFIX: $prefix"
echo "            Signals: $ac_cv_func_sigaction"
echo ""

