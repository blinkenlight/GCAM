/**
 *  gcode_bolt_holes.h
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

#ifndef _GCODE_BOLT_HOLES_H
#define _GCODE_BOLT_HOLES_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_BOLT_HOLES_EXTRUSION        0x00
#define GCODE_BIN_DATA_BOLT_HOLES_POSITION         0x01
#define GCODE_BIN_DATA_BOLT_HOLES_HOLE_DIAMETER    0x02
#define GCODE_BIN_DATA_BOLT_HOLES_OFFSET_DISTANCE  0x03
#define GCODE_BIN_DATA_BOLT_HOLES_TYPE             0x04
#define GCODE_BIN_DATA_BOLT_HOLES_NUMBER           0x05
#define GCODE_BIN_DATA_BOLT_HOLES_OFFSET_ANGLE     0x06
#define GCODE_BIN_DATA_BOLT_HOLES_POCKET           0x07

#define GCODE_BOLT_HOLES_TYPE_RADIAL               0x00
#define GCODE_BOLT_HOLES_TYPE_MATRIX               0x01

static const char *GCODE_XML_ATTR_BOLT_HOLES_POSITION = "position";
static const char *GCODE_XML_ATTR_BOLT_HOLES_HOLE_DIAMETER = "hole-diameter";
static const char *GCODE_XML_ATTR_BOLT_HOLES_OFFSET_DISTANCE = "offset-distance";
static const char *GCODE_XML_ATTR_BOLT_HOLES_TYPE = "type";
static const char *GCODE_XML_ATTR_BOLT_HOLES_NUMBER = "number";
static const char *GCODE_XML_ATTR_BOLT_HOLES_OFFSET_ANGLE = "offset-angle";
static const char *GCODE_XML_ATTR_BOLT_HOLES_POCKET = "pocket";

typedef struct gcode_bolt_holes_s
{
  gcode_offset_t offset;
  gcode_vec2d_t position;
  int number[2];                                                                /* number[0] used in radial, number[0] AND number[1] used in matrix */
  uint8_t type;
  gfloat_t hole_diameter;
  gfloat_t offset_distance;
  gfloat_t offset_angle;
  uint8_t pocket;
} gcode_bolt_holes_t;

void gcode_bolt_holes_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_bolt_holes_free (gcode_block_t **block);
void gcode_bolt_holes_save (gcode_block_t *block, FILE *fh);
void gcode_bolt_holes_load (gcode_block_t *block, FILE *fh);
void gcode_bolt_holes_make (gcode_block_t *block);
void gcode_bolt_holes_draw (gcode_block_t *block, gcode_block_t *selected);
void gcode_bolt_holes_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max, uint8_t mode);
void gcode_bolt_holes_move (gcode_block_t *block, gcode_vec2d_t delta);
void gcode_bolt_holes_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_bolt_holes_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_bolt_holes_scale (gcode_block_t *block, gfloat_t scale);
void gcode_bolt_holes_parse (gcode_block_t *block, const char **xmlattr);
void gcode_bolt_holes_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_bolt_holes_rebuild (gcode_block_t *block);

#endif
