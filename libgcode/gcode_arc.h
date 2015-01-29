/**
 *  gcode_arc.h
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

#ifndef _GCODE_ARC_H
#define _GCODE_ARC_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_ARC_START_POINT  0x00
#define GCODE_BIN_DATA_ARC_RADIUS       0x01
#define GCODE_BIN_DATA_ARC_START_ANGLE  0x02
#define GCODE_BIN_DATA_ARC_SWEEP_ANGLE  0x03
#define GCODE_BIN_DATA_ARC_INTERFACE    0x04

#define GCODE_ARC_INTERFACE_SWEEP       0x0
#define GCODE_ARC_INTERFACE_RADIUS      0x1
#define GCODE_ARC_INTERFACE_CENTER      0x2

static const char *GCODE_XML_ATTR_ARC_START_POINT = "start-point";
static const char *GCODE_XML_ATTR_ARC_RADIUS = "radius";
static const char *GCODE_XML_ATTR_ARC_START_ANGLE = "start-angle";
static const char *GCODE_XML_ATTR_ARC_SWEEP_ANGLE = "sweep-angle";
static const char *GCODE_XML_ATTR_ARC_INTERFACE = "interface";

typedef struct gcode_arc_s
{
  gcode_vec2d_t p;
  gfloat_t radius;
  gfloat_t start_angle;
  gfloat_t sweep_angle;
  uint8_t native_mode;
} gcode_arc_t;

typedef struct gcode_arcdata_s
{
  gcode_vec2d_t p0;
  gcode_vec2d_t p1;
  gcode_vec2d_t cp;
  gfloat_t radius;
  gfloat_t start_angle;
  gfloat_t sweep_angle;
  uint8_t fla;
  uint8_t fls;
} gcode_arcdata_t;

void gcode_arc_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_arc_free (gcode_block_t **block);
void gcode_arc_save (gcode_block_t *block, FILE *fh);
void gcode_arc_load (gcode_block_t *block, FILE *fh);
void gcode_arc_make (gcode_block_t *block);
void gcode_arc_draw (gcode_block_t *block, gcode_block_t *selected);
int gcode_arc_eval (gcode_block_t *block, gfloat_t y, gfloat_t *x_array, uint32_t *x_index);
int gcode_arc_ends (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, uint8_t mode);
int gcode_arc_center (gcode_block_t *block, gcode_vec2d_t center, uint8_t mode);
void gcode_arc_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max);
gfloat_t gcode_arc_length (gcode_block_t *block);
void gcode_arc_move (gcode_block_t *block, gcode_vec2d_t delta);
void gcode_arc_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle);
void gcode_arc_scale (gcode_block_t *block, gfloat_t scale);
void gcode_arc_parse (gcode_block_t *block, const char **xmlattr);
void gcode_arc_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_arc_with_offset (gcode_block_t *block, gcode_vec2d_t origin, gcode_vec2d_t center, gcode_vec2d_t p0, gfloat_t *arc_radius_offset, gfloat_t *start_angle);
void gcode_arc_flip_direction (gcode_block_t *block);
int gcode_arc_radius_to_sweep (gcode_arcdata_t *arc);
int gcode_arc_center_to_sweep (gcode_arcdata_t *arc);

#endif
