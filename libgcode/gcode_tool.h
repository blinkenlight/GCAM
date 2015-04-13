/**
 *  gcode_tool.h
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

#ifndef _GCODE_TOOL_H
#define _GCODE_TOOL_H

#include "gcode_util.h"
#include "gcode_internal.h"

#define GCODE_BIN_DATA_TOOL_DIAMETER         0x00
#define GCODE_BIN_DATA_TOOL_LENGTH           0x01
#define GCODE_BIN_DATA_TOOL_PROMPT           0x02
#define GCODE_BIN_DATA_TOOL_LABEL            0x03
#define GCODE_BIN_DATA_TOOL_FEED             0x04
#define GCODE_BIN_DATA_TOOL_CHANGE_POSITION  0x05
#define GCODE_BIN_DATA_TOOL_NUMBER           0x06
#define GCODE_BIN_DATA_TOOL_PLUNGE_RATIO     0x07
#define GCODE_BIN_DATA_TOOL_SPINDLE_RPM      0x08
#define GCODE_BIN_DATA_TOOL_COOLANT          0x09

static const char *GCODE_XML_ATTR_TOOL_DIAMETER = "diameter";
static const char *GCODE_XML_ATTR_TOOL_LENGTH = "length";
static const char *GCODE_XML_ATTR_TOOL_PROMPT = "prompt";
static const char *GCODE_XML_ATTR_TOOL_LABEL = "label";
static const char *GCODE_XML_ATTR_TOOL_FEED = "feed";
static const char *GCODE_XML_ATTR_TOOL_CHANGE_POSITION = "change-position";
static const char *GCODE_XML_ATTR_TOOL_NUMBER = "number";
static const char *GCODE_XML_ATTR_TOOL_PLUNGE_RATIO = "plunge-ratio";
static const char *GCODE_XML_ATTR_TOOL_SPINDLE_RPM = "spindle-rpm";
static const char *GCODE_XML_ATTR_TOOL_COOLANT = "coolant";

typedef struct gcode_tool_s
{
  gfloat_t diameter;
  gfloat_t length;
  uint8_t prompt;
  char label[32];
  gfloat_t hinc;
  gfloat_t vinc;
  gfloat_t feed;
  gfloat_t change_position[3];
  uint8_t number;
  gfloat_t plunge_ratio;
  uint32_t spindle_rpm;
  uint8_t coolant;
} gcode_tool_t;

void gcode_tool_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_tool_free (gcode_block_t **block);
void gcode_tool_save (gcode_block_t *block, FILE *fh);
void gcode_tool_load (gcode_block_t *block, FILE *fh);
void gcode_tool_make (gcode_block_t *block);
void gcode_tool_scale (gcode_block_t *block, gfloat_t scale);
void gcode_tool_parse (gcode_block_t *block, const char **xmlattr);
void gcode_tool_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model);
void gcode_tool_calc (gcode_block_t *block);
gcode_tool_t *gcode_tool_find (gcode_block_t *block);

#endif
