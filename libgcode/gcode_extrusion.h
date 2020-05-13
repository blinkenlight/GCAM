/**
 *  gcode_extrusion.h
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

#ifndef _GCODE_EXTRUSION_H
#define _GCODE_EXTRUSION_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_EXTRUSION_NUMBER      0x00
#define GCODE_BIN_DATA_EXTRUSION_RESOLUTION  0x01
#define GCODE_BIN_DATA_EXTRUSION_CUT_SIDE    0x02

#define GCODE_EXTRUSION_INSIDE               0x0
#define GCODE_EXTRUSION_OUTSIDE              0x1
#define GCODE_EXTRUSION_ALONG                0x2

static const char *GCODE_XML_ATTR_EXTRUSION_RESOLUTION = "resolution";
static const char *GCODE_XML_ATTR_EXTRUSION_CUT_SIDE = "cut-side";

typedef struct gcode_extrusion_s
{
  gcode_offset_t offset;
  gfloat_t resolution;
  uint8_t cut_side;
} gcode_extrusion_t;

void gcode_extrusion_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_extrusion_free (gcode_block_t **block);
void gcode_extrusion_save (gcode_block_t *block, FILE *fh);
void gcode_extrusion_load (gcode_block_t *block, FILE *fh);
void gcode_extrusion_make (gcode_block_t *block);
void gcode_extrusion_draw (gcode_block_t *block, gcode_block_t *selected);
int gcode_extrusion_ends (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, uint8_t mode);
void gcode_extrusion_scale (gcode_block_t *block, gfloat_t scale);
void gcode_extrusion_parse (gcode_block_t *block, const char **xmlattr);
void gcode_extrusion_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
int gcode_extrusion_evaluate_offset (gcode_block_t *block, gfloat_t z, gfloat_t *offset);
int gcode_extrusion_taper_exists (gcode_block_t *block);

#endif
