/**
 *  gcode_image.h
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

#ifndef _GCODE_IMAGE_H
#define _GCODE_IMAGE_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_IMAGE_RESOLUTION  0x00
#define GCODE_BIN_DATA_IMAGE_SIZE        0x01
#define GCODE_BIN_DATA_IMAGE_DMAP        0x02

static const char *GCODE_XML_ATTR_IMAGE_RESOLUTION = "resolution";
static const char *GCODE_XML_ATTR_IMAGE_SIZE = "size";

typedef struct gcode_image_s
{
  int resolution[2];
  gcode_vec3d_t size;
  gfloat_t *dmap;                                                               /* depth map */
} gcode_image_t;

void gcode_image_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_image_free (gcode_block_t **block);
void gcode_image_save (gcode_block_t *block, FILE *fh);
void gcode_image_load (gcode_block_t *block, FILE *fh);
void gcode_image_make (gcode_block_t *block);
void gcode_image_draw (gcode_block_t *block, gcode_block_t *selected);
void gcode_image_scale (gcode_block_t *block, gfloat_t scale);
void gcode_image_parse (gcode_block_t *block, const char **xmlattr);
void gcode_image_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_image_open (gcode_block_t *block, char *filename);

#endif
