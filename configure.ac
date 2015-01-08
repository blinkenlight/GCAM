########################################################################
#  configure.ac
#  Source code file for G-Code generation, simulation, and visualization
#  library.
#
#  Copyright (C) 2006 - 2010 by Justin Shumaker
#  Copyright (C) 2014 by Asztalos Attila Oszkár
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
########################################################################
AC_INIT
AC_CANONICAL_SYSTEM
AC_CONFIG_SRCDIR([gcam.c])
AC_CONFIG_MACRO_DIR([m4])
AC_PREFIX_DEFAULT([/usr/local/gcam])
AM_INIT_AUTOMAKE(gcam, 2014.05.23)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

## Set the prefix to the default prefix if not defined.
if test "x$prefix" = "xNONE" ; then
        prefix="$ac_default_prefix"
fi


##
## Windows Console
##
AC_MSG_CHECKING(whether to enable windows console)
want_windows_console=yes
AC_ARG_ENABLE(windows-console,
        AC_HELP_STRING(--disable-windows-console, [Build with windows console [default=yes]]),
        [
                if test "x$enableval" = "xno"
                then
                        want_windows_console=no
                        extra_C_flags="-mwindows"
                fi
        ]
)

AC_MSG_RESULT($want_windows_console)


##
## Debugging
##
AC_MSG_CHECKING(whether to compile for debugging)
want_debug=no
AC_ARG_ENABLE(debug,
        AC_HELP_STRING(--enable-debug, [Build with debugging [default=no]]),
        [
                if test "x$enableval" = "xyes"
                then
                        want_debug=yes
                fi
        ]
)

AC_MSG_RESULT($want_debug)

if test "x$want_debug" = "xyes"
then
        export CFLAGS="-O0 -std=gnu99 -g -ggdb -Wall -Wmissing-prototypes -Wunused-function -Wno-unused-result -Wno-unused-variable -Wno-unused-but-set-variable -pedantic -pg $extra_C_flags"
else
        export CFLAGS="-O3 -std=gnu99 -Wmissing-prototypes -Wno-unused-function -Wno-unused-result -Wno-unused-variable -Wno-unused-but-set-variable -pedantic -ffast-math -fomit-frame-pointer $extra_C_flags"
fi

## Support for European mantissa issues concerning ',' and '.'
export LC_NUMERIC="POSIX"

##
## GTK
##
AM_PATH_GTK_2_0(2.10.0,,AC_MSG_ERROR([GTK+ 2.10 or higher is required]))
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

## AC_DEFINE_UNQUOTED([GTK_MAJOR_VERSION], [$gtk_config_major_version], GTK Major Version)
## AC_DEFINE_UNQUOTED([GTK_MINOR_VERSION], [$gtk_config_minor_version], GTK Minor Version)

#AM_PATH_GLIB(1.2.0,,AC_MSG_ERROR("gthread 2.0 is required"), gthread)
#AC_SUBST(GLIB_CFLAGS)
#AC_SUBST(GLIB_LIBS)

##
## GTK GL Extension
##
AM_PATH_GTKGLEXT_1_0(1.0.0,,AC_MSG_ERROR([GTK GL Extension is required]))
AC_SUBST(GTKGLEXT_CFLAGS)
AC_SUBST(GTKGLEXT_LIBS)

##
## Lib PNG
##
AC_MSG_CHECKING([for PNG])
PNG_LIBS=`pkg-config libpng --silence-errors --libs`
if test "x$PNG_LIBS" = "x"; then
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([libPNG development files required.])
else
	AC_MSG_RESULT([yes])
	AC_SUBST(PNG_LIBS)
fi

##
## Define the configuration date
##
AC_DEFINE_UNQUOTED([CONFIG_DATE], ["$(date +%Y.%m.%d)"], Config date)

AC_CONFIG_FILES([Makefile \
		libgcode/Makefile \
		libgui/Makefile \
		samples/Makefile \
		share/Makefile])
AC_OUTPUT
