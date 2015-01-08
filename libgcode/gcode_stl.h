/**
 *  gcode_stl.h
 *  Source code file for G-Code generation, simulation, and visualization
 *  library.
 *
 *  Copyright (C) 2006 - 2010 by Justin Shumaker
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

#ifndef _GCODE_STL_H
#define _GCODE_STL_H

#include "gcode_internal.h"

typedef struct gcode_stl_s
{
  gcode_offset_t offset;
  gcode_block_t **slice_list;
  int tri_num;
  float *tri_list;
  int slices;
  int alloc_slices;
} gcode_stl_t;

void gcode_stl_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_stl_free (gcode_block_t **block);
void gcode_stl_make (gcode_block_t *block);
void gcode_stl_save (gcode_block_t *block, FILE *fh);
void gcode_stl_load (gcode_block_t *block, FILE *fh);
void gcode_stl_draw (gcode_block_t *block, gcode_block_t *selected);
void gcode_stl_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_stl_scale (gcode_block_t *block, gfloat_t scale);
void gcode_stl_build (gcode_block_t *block);
int gcode_stl_import (gcode_block_t *block, char *filename);
void gcode_stl_generate_slice_contours (gcode_block_t *block);

#endif
