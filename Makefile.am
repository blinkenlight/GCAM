########################################################################
#  Makefile.am
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
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
########################################################################
bin_PROGRAMS = gcam

if HAVE_WINDRES
gcam_SOURCES = gcam.c gcam-icon.rc
else
gcam_SOURCES = gcam.c
endif

gcam_LDFLAGS = -static

gcam_LDADD = \
	$(top_builddir)/libgui/libgui.la \
	$(top_builddir)/libgcode/libgcode.la \
	$(AM_LDFLAGS)

ACLOCAL_AMFLAGS = -I m4 

AM_CFLAGS = \
	@GTKGLEXT_CFLAGS@ \
	-I${top_srcdir}/libgui \
	-I${top_srcdir}/libgcode

AM_LDFLAGS = \
	${top_builddir}/libgui/libgui.la \
	${top_builddir}/libgcode/libgcode.la \
	@GTK_LIBS@ @GTKGLEXT_LIBS@ @PNG_LIBS@ -lexpat -lm

SUBDIRS = \
	libgui \
	libgcode \
	samples \
	share

SUFFIXES: .rc

.rc.o:
	$(WINDRES) $< -o $@
