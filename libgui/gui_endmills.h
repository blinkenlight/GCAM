/**
 *  gui_endmills.h
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

#ifndef _GUI_ENDMILLS_H
#define _GUI_ENDMILLS_H

#include "gcode.h"

#define GUI_ENDMILL_INTERNAL   0x0
#define GUI_ENDMILL_EXTERNAL   0x1

static const char *GCODE_XML_ENDMILLS_FILENAME = "endmills.xml";

static const char *GCODE_XML_TAG_ENDMILL = "endmill";

static const char *GCODE_XML_ATTR_ENDMILL_NUMBER = "number";
static const char *GCODE_XML_ATTR_ENDMILL_TYPE = "type";
static const char *GCODE_XML_ATTR_ENDMILL_DIAMETER = "diameter";
static const char *GCODE_XML_ATTR_ENDMILL_UNIT = "unit";
static const char *GCODE_XML_ATTR_ENDMILL_DESCRIPTION = "description";

static const char *GCODE_XML_VAL_ENDMILL_UNIT_INCH = "inch";
static const char *GCODE_XML_VAL_ENDMILL_UNIT_MILLIMETER = "millimeter";

typedef struct gui_endmill_s
{
  uint8_t number;
  gfloat_t diameter;
  uint8_t unit;
  uint8_t origin;
  char description[64];
} gui_endmill_t;

typedef struct gui_endmill_list_s
{
  gui_endmill_t *endmill;
  uint8_t number;
} gui_endmill_list_t;

void gui_endmills_init (gui_endmill_list_t *endmill_list);
void gui_endmills_free (gui_endmill_list_t *endmill_list);
int gui_endmills_read (gui_endmill_list_t *endmill_list);
gfloat_t gui_endmills_size (gui_endmill_t *endmill, uint8_t unit);
gui_endmill_t *gui_endmills_find (gui_endmill_list_t *endmill_list, char *description, uint8_t fallback);
void gui_endmills_tack (gui_endmill_list_t *endmill_list, uint8_t number, gfloat_t diameter, uint8_t unit, char *description);
void gui_endmills_cull (gui_endmill_list_t *endmill_list);

#endif
