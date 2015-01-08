/**
 *  gcode_begin.h
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

#ifndef _GCODE_BEGIN_H
#define _GCODE_BEGIN_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_BEGIN_COORDINATE_SYSTEM    0x00

#define GCODE_BEGIN_COORDINATE_SYSTEM_NONE        0x0
#define GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE1  0x1
#define GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE2  0x2
#define GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE3  0x3
#define GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE4  0x4
#define GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE5  0x5
#define GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE6  0x6

static const char *GCODE_XML_ATTR_BEGIN_COORDINATE_SYSTEM = "coordinate-system";

typedef struct gcode_begin_s
{
  uint8_t coordinate_system;
} gcode_begin_t;

void gcode_begin_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_begin_free (gcode_block_t **block);
void gcode_begin_make (gcode_block_t *block);
void gcode_begin_save (gcode_block_t *block, FILE *fh);
void gcode_begin_load (gcode_block_t *block, FILE *fh);
void gcode_begin_parse (gcode_block_t *block, const char **xmlattr);

#endif
