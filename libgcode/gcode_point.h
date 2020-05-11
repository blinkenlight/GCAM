/**
 *  gcode_point.h
 *  Source code file for G-Code generation, simulation, and visualization
 *  library.
 *
 *  Copyright (C) 2006 - 2010 by Justin Shumaker
 *  Copyright (C) 2014 - 2020 by Asztalos Attila Oszkár
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

#ifndef _GCODE_POINT_H
#define _GCODE_POINT_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_POINT_POSITION  0x00

static const char *GCODE_XML_ATTR_POINT_POSITION = "position";

typedef struct gcode_point_s
{
  gcode_vec2d_t p;
} gcode_point_t;

void gcode_point_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_point_free (gcode_block_t **block);
void gcode_point_save (gcode_block_t *block, FILE *fh);
void gcode_point_load (gcode_block_t *block, FILE *fh);
void gcode_point_draw (gcode_block_t *block, gcode_block_t *selected);
void gcode_point_move (gcode_block_t *block, gcode_vec2d_t delta);
void gcode_point_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_point_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_point_scale (gcode_block_t *block, gfloat_t scale);
void gcode_point_parse (gcode_block_t *block, const char **xmlattr);
void gcode_point_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_point_with_offset (gcode_block_t *block, gcode_vec2d_t p);

#endif
