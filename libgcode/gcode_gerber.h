/**
 *  gcode_gerber.h
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

#ifndef _GCODE_GERBER_H
#define _GCODE_GERBER_H

#include "gcode_internal.h"

#define GCODE_GERBER_APERTURE_TYPE_CIRCLE     0x00
#define GCODE_GERBER_APERTURE_TYPE_RECTANGLE  0x01
#define GCODE_GERBER_APERTURE_TYPE_OBROUND    0x02
#define GCODE_GERBER_APERTURE_TYPE_ROUNDRECT  0x03

#define GCODE_GERBER_TRACE_TYPE_LINE          0x00
#define GCODE_GERBER_TRACE_TYPE_ARC           0x01

#define GCODE_GERBER_ARC_CCW                  0x00
#define GCODE_GERBER_ARC_CW                   0x01

typedef struct gcode_gerber_aperture_s
{
  uint8_t type;                                                                 /* Circle, Rectangle or Obround */
  uint8_t ind;                                                                  /* Wheel Position */
  gcode_vec2d_t v;                                                              /* v[0] = diameter if CIRCLE, v[0] = x and v[1] = y if RECTANGLE or OBROUND */
  gfloat_t r;                                                                   /* Rounding radius - only contains data for rounded rectangles */
} gcode_gerber_aperture_t;

typedef struct gcode_gerber_exposure_s
{
  uint8_t type;
  gcode_vec2d_t pos;
  gcode_vec2d_t v;                                                              /* v[0] = diameter if CIRCLE, v[0] = x and v[1] = y if RECTANGLE or OBROUND */
  gfloat_t r;                                                                   /* Rounding radius - only contains data for rounded rectangles */
} gcode_gerber_exposure_t;

typedef struct gcode_gerber_polygon_s
{
  int segment_count;                                                            /* Number of polygon segments processed, not including zero-length ones */
  int side_count[2];                                                            /* Number of side-size segments found, where [0] = horizontal and [1] = vertical */
  gcode_vec2d_t initial;                                                        /* Initial starting point of the polygon, [0] = x and [1] = y */
  gcode_vec2d_t current;                                                        /* Current endpoint of the polygon, [0] = x and [1] = y */
  gcode_vec2d_t side_max;                                                       /* Size of the longest segment found, where [0] = horizontal and [1] = vertical */
  gcode_vec2d_t aabb_min;                                                       /* Bottom left corner of the axis-aligned bounding box of the polygon */
  gcode_vec2d_t aabb_max;                                                       /* Top right corner of the axis-aligned bounding box of the polygon */
} gcode_gerber_polygon_t;

typedef struct gcode_gerber_trace_s
{
  uint8_t type;                                                                 /* Line or Arc */
  gcode_vec2d_t p0;
  gcode_vec2d_t p1;
  gcode_vec2d_t cp;                                                             /* Only contains data for arcs */
  gfloat_t start_angle;                                                         /* Only contains data for arcs */
  gfloat_t sweep_angle;                                                         /* Only contains data for arcs */
  gfloat_t radius;                                                              /* Only contains data for arcs */
  gfloat_t width;
} gcode_gerber_trace_t;

int gcode_gerber_import (gcode_block_t *sketch_block, char *filename, gfloat_t depth, gfloat_t offset);

#endif
