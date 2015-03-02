/**
 *  gcode_line.h
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

#ifndef _GCODE_LINE_H
#define _GCODE_LINE_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_LINE_POINTS  0x00

static const char *GCODE_XML_ATTR_LINE_START_POINT = "start-point";
static const char *GCODE_XML_ATTR_LINE_END_POINT = "end-point";

typedef struct gcode_line_s
{
  gcode_vec2d_t p0;
  gcode_vec2d_t p1;
} gcode_line_t;

void gcode_line_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_line_free (gcode_block_t **block);
void gcode_line_save (gcode_block_t *block, FILE *fh);
void gcode_line_load (gcode_block_t *block, FILE *fh);
void gcode_line_make (gcode_block_t *block);
void gcode_line_draw (gcode_block_t *block, gcode_block_t *selected);
int gcode_line_eval (gcode_block_t *block, gfloat_t y, gfloat_t *x_array, uint32_t *x_index);
int gcode_line_ends (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, uint8_t mode);
void gcode_line_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max);
void gcode_line_qdbb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max);
gfloat_t gcode_line_length (gcode_block_t *block);
void gcode_line_move (gcode_block_t *block, gcode_vec2d_t delta);
void gcode_line_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_line_scale (gcode_block_t *block, gfloat_t scale);
void gcode_line_parse (gcode_block_t *block, const char **xmlattr);
void gcode_line_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_line_with_offset (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, gcode_vec2d_t normal);
void gcode_line_flip_direction (gcode_block_t *block);

#endif
