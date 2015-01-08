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

noinst_LTLIBRARIES = libgui.la

libgui_la_SOURCES = \
	gui.c \
	gui_endmills.c \
	gui_machines.c \
	gui_menu.c \
	gui_menu_file.c \
	gui_menu_edit.c \
	gui_menu_insert.c \
	gui_menu_assistant.c \
	gui_menu_view.c \
	gui_menu_help.c \
	gui_menu_util.c \
	gui_opengl.c \
	gui_settings.c \
	gui_tab.c

AM_CFLAGS = \
	@GTK_CFLAGS@ @GTKGLEXT_CFLAGS@ \
	-I${top_srcdir}/libgcode \
	-DBUILD_DATE=\"`date +%Y.%m.%d`\"

AM_CPPFLAGS = -DSHARE_PREFIX=\"$(pkgdatadir)/\"

include_HEADERS = \
	gui.h \
	gui_define.h \
	gui_endmills.h \
	gui_machines.h \
	gui_menu.h \
	gui_menu_file.h \
	gui_menu_edit.h \
	gui_menu_insert.h \
	gui_menu_assistant.h \
	gui_menu_view.h \
	gui_menu_help.h \
	gui_menu_util.h \
	gui_opengl.h \
	gui_settings.h \
	gui_tab.h \
	gcam_icon.h
