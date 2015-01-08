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

typedef struct gcode_gerber_exposure_s
{
  uint8_t type;
  gcode_vec2d_t pos;
  gfloat_t v[2];                                                                /* v[0] = diameter if CIRCLE, otherwise v[0] = x and v[1] = y if RECTANGLE */
} gcode_gerber_exposure_t;

typedef struct gcode_gerber_trace_s
{
  gcode_vec2d_t p0;
  gcode_vec2d_t p1;
  gfloat_t radius;
} gcode_gerber_trace_t;

typedef struct gcode_gerber_aperture_s
{
  uint8_t type;                                                                 /* Circle or Rectangle */
  uint8_t ind;                                                                  /* Wheel Position */
  gfloat_t v[2];                                                                /* v[0] = diameter if CIRCLE, otherwise v[0] = x and v[1] = y if RECTANGLE */
} gcode_gerber_aperture_t;

int gcode_gerber_import (gcode_block_t *sketch_block, char *filename, gfloat_t depth, gfloat_t offset);

#endif
