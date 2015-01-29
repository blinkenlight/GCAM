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

noinst_LTLIBRARIES = libgcode.la

libgcode_la_SOURCES = \
	gcode.c \
	gcode_arc.c \
	gcode_begin.c \
	gcode_bolt_holes.c \
	gcode_code.c \
	gcode_drill_holes.c \
	gcode_end.c \
	gcode_excellon.c \
	gcode_extrusion.c \
	gcode_gerber.c \
	gcode_image.c \
	gcode_internal.c \
	gcode_line.c \
	gcode_math.c \
	gcode_pocket.c \
	gcode_point.c \
	gcode_sim.c \
	gcode_sketch.c \
	gcode_stl.c \
	gcode_svg.c \
	gcode_template.c \
	gcode_tool.c \
	gcode_util.c

AM_CFLAGS = \
	@GTKGLEXT_CFLAGS@ \
  -I${top_srcdir}/libgui

include_HEADERS = \
	gcode.h \
	gcode_arc.h \
	gcode_begin.h \
	gcode_bolt_holes.h \
	gcode_code.h \
	gcode_drill_holes.h \
	gcode_end.h \
	gcode_extrusion.h \
	gcode_excellon.h \
	gcode_gerber.h \
	gcode_image.h \
	gcode_internal.h \
	gcode_line.h \
	gcode_math.h \
	gcode_pocket.h \
	gcode_point.h \
	gcode_sim.h \
	gcode_sketch.h \
	gcode_stl.h \
	gcode_svg.h \
	gcode_template.h \
	gcode_tool.h \
	gcode_util.h
