/**
 *  gcode_template.h
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

#ifndef _GCODE_TEMPLATE_H
#define _GCODE_TEMPLATE_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_TEMPLATE_NUMBER    0x00
#define GCODE_BIN_DATA_TEMPLATE_POSITION  0x01
#define GCODE_BIN_DATA_TEMPLATE_ROTATION  0x02

static const char *GCODE_XML_ATTR_TEMPLATE_POSITION = "position";
static const char *GCODE_XML_ATTR_TEMPLATE_ROTATION = "rotation";

typedef struct gcode_template_s
{
  gcode_offset_t offset;
  gcode_vec2d_t position;
  gfloat_t rotation;
} gcode_template_t;

void gcode_template_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_template_free (gcode_block_t **block);
void gcode_template_save (gcode_block_t *block, FILE *fh);
void gcode_template_load (gcode_block_t *block, FILE *fh);
void gcode_template_make (gcode_block_t *block);
void gcode_template_draw (gcode_block_t *block, gcode_block_t *selected);
void gcode_template_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max, uint8_t mode);
void gcode_template_move (gcode_block_t *block, gcode_vec2d_t delta);
void gcode_template_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_template_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_template_scale (gcode_block_t *block, gfloat_t scale);
void gcode_template_parse (gcode_block_t *block, const char **xmlattr);
void gcode_template_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);

#endif
