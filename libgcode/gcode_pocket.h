/**
 *  gcode_pocket.h
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

#ifndef _GCODE_POCKET_H
#define _GCODE_POCKET_H

#include "gcode_internal.h"
#include "gcode_tool.h"

#define PADDING_FRACTION  0.1

typedef struct gcode_pocket_row_s
{
  int line_count;
  gcode_vec2d_t *line_array;
  gfloat_t y;
} gcode_pocket_row_t;

typedef struct gcode_pocket_s
{
  int row_count;
  int seg_count;
  gcode_tool_t *tool;
  gcode_block_t *target;
  gcode_block_t *first_block;
  gcode_block_t *final_block;
  gcode_pocket_row_t *row_array;
} gcode_pocket_t;

void gcode_pocket_init (gcode_pocket_t *pocket, gcode_block_t *target, gcode_tool_t *tool);
void gcode_pocket_free (gcode_pocket_t *pocket);
void gcode_pocket_prep (gcode_pocket_t *pocket, gcode_block_t *first_block, gcode_block_t *final_block);
void gcode_pocket_make (gcode_pocket_t *pocket, gfloat_t z, gfloat_t touch_z);
void gcode_pocket_subtract (gcode_pocket_t *pocket_a, gcode_pocket_t *pocket_b);

#endif
