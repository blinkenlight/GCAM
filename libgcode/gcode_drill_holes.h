/**
 *  gcode_drill_holes.h
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

#ifndef _GCODE_DRILL_HOLES_H
#define _GCODE_DRILL_HOLES_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_DRILL_HOLES_NUMBER        0x00
#define GCODE_BIN_DATA_DRILL_HOLES_DEPTH         0x01
#define GCODE_BIN_DATA_DRILL_HOLES_INCREMENT     0x02
#define GCODE_BIN_DATA_DRILL_HOLES_OPTIMAL_PATH  0x03

static const char *GCODE_XML_ATTR_DRILL_HOLES_DEPTH = "depth";
static const char *GCODE_XML_ATTR_DRILL_HOLES_INCREMENT = "increment";
static const char *GCODE_XML_ATTR_DRILL_HOLES_OPTIMAL_PATH = "optimal-path";

typedef struct gcode_drill_holes_s
{
  gcode_offset_t offset;
  gfloat_t depth;
  gfloat_t increment;
  uint8_t optimal_path;
} gcode_drill_holes_t;

void gcode_drill_holes_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_drill_holes_free (gcode_block_t **block);
void gcode_drill_holes_make (gcode_block_t *block);
void gcode_drill_holes_save (gcode_block_t *block, FILE *fh);
void gcode_drill_holes_load (gcode_block_t *block, FILE *fh);
void gcode_drill_holes_draw (gcode_block_t *block, gcode_block_t *selected);
void gcode_drill_holes_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_drill_holes_scale (gcode_block_t *block, gfloat_t scale);
void gcode_drill_holes_parse (gcode_block_t *block, const char **xmlattr);
void gcode_drill_holes_pattern (gcode_block_t *block, uint32_t iterations, gfloat_t translate_x, gfloat_t translate_y, gfloat_t rotate_about_x, gfloat_t rotate_about_y, gfloat_t rotation);

#endif
