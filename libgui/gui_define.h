/**
 *  gui_define.h
 *  Source code file for G-Code generation, simulation, and visualization
 *  library.
 *
 *  Copyright (C) 2006 - 2010 by Justin Shumaker
 *  Copyright (C) 2014 by Asztalos Attila Oszkár
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GUI_DEFINE_H
#define _GUI_DEFINE_H

#include "gcode_math.h"

/* *INDENT-OFF* */

static const gfloat_t GCODE_OPENGL_BACKGROUND_COLORS[4][3] = { { 0.12, 0.12, 0.12 },
                                                               { 0.14, 0.14, 0.14 },
                                                               { 0.28, 0.28, 0.28 },
                                                               { 0.18, 0.18, 0.18 } };
static const gfloat_t GCODE_OPENGL_SELECTABLE_COLORS[2][3] = { { 0.90, 0.82, 0.72 },
                                                               { 1.00, 0.50, 0.20 } };
/* *INDENT-ON* */

static const gfloat_t GCODE_OPENGL_SMALL_POINT_COLOR[3] = { 1.00, 1.00, 0.00 };
static const gfloat_t GCODE_OPENGL_BREAK_POINT_COLOR[3] = { 1.00, 0.00, 0.00 };
static const gfloat_t GCODE_OPENGL_DATUM_POINT_COLOR[3] = { 0.00, 1.00, 1.00 };
static const gfloat_t GCODE_OPENGL_STL_SURFACE_COLOR[3] = { 0.30, 0.90, 0.30 };
static const gfloat_t GCODE_OPENGL_FINEST_GRID_COLOR[3] = { 0.35, 0.35, 0.35 };
static const gfloat_t GCODE_OPENGL_MEDIUM_GRID_COLOR[3] = { 0.55, 0.55, 0.55 };
static const gfloat_t GCODE_OPENGL_COARSE_GRID_COLOR[3] = { 0.85, 0.85, 0.85 };
static const gfloat_t GCODE_OPENGL_GRID_BORDER_COLOR[3] = { 0.70, 0.70, 0.70 };
static const gfloat_t GCODE_OPENGL_GRID_ORIGIN_COLOR[3] = { 0.70, 0.20, 0.20 };

static const gfloat_t GCODE_OPENGL_SMALL_POINT_SIZE = 5;
static const gfloat_t GCODE_OPENGL_BREAK_POINT_SIZE = 7;
static const gfloat_t GCODE_OPENGL_DATUM_POINT_SIZE = 7;
static const gfloat_t GCODE_OPENGL_VOXEL_POINT_SIZE = 2;

#define PROJECT_CLOSED            0x0
#define PROJECT_OPEN              0x1

#define GUI_INSERT_AFTER          1
#define GUI_INSERT_UNDER          2
#define GUI_APPEND_UNDER          4
#define GUI_ADD_AS_CHILD          6
#define GUI_ADD_AFTER_OR_UNDER    7
#define GUI_INSERT_WITH_TANGENCY  8

#define WINDOW_W                  720
#define WINDOW_H                  540

#define PANEL_LEFT_W              280
#define PANEL_LEFT_H              (WINDOW_H - 80)                               /* Subtract away the menu and status bar */
#define PANEL_LEFT_W_SW           (PANEL_LEFT_W - 20)                           /* Left panel minus margins for scroll window */
#define PANEL_BOTTOM_H            260

#define TABLE_SPACING             3

#define MANTISSA                  5                                             /* Length of Mantissa */

#define MAX_DIM_X                 500.0                                         /* Maximum material X size, Inches */
#define MAX_DIM_Y                 500.0                                         /* Maximum material Y size, Inches */
#define MAX_DIM_Z                 50.0                                          /* Maximum material Z size, Inches */

#define MAX_CLR_Z                 4.0                                           /* Maximum traverse / clearance Z height, Inches */

#define DEF_UNITS                 GCODE_UNITS_INCH

#endif
