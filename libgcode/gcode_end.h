/**
 *  gcode_end.h
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

#ifndef _GCODE_END_H
#define _GCODE_END_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_END_RETRACT_POSITION  0x00
#define GCODE_BIN_DATA_END_HOME_ALL_AXES     0x01

static const char *GCODE_XML_ATTR_END_RETRACT_POSITION = "retract-position";
static const char *GCODE_XML_ATTR_END_HOME_ALL_AXES = "home-all-axes";

typedef struct gcode_end_s
{
  gcode_vec3d_t retract_position;
  uint8_t home_all_axes;                                                        /* only if G28 is supported */
} gcode_end_t;

void gcode_end_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_end_free (gcode_block_t **block);
void gcode_end_save (gcode_block_t *block, FILE *fh);
void gcode_end_load (gcode_block_t *block, FILE *fh);
void gcode_end_make (gcode_block_t *block);
void gcode_end_scale (gcode_block_t *block, gfloat_t scale);
void gcode_end_parse (gcode_block_t *block, const char **xmlattr);

#endif
