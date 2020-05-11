/**
 *  gui_settings.h
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

#ifndef _GUI_SETTINGS_H
#define _GUI_SETTINGS_H

#include "gcode.h"

static const char *GCODE_XML_SETTINGS_FILENAME = "settings.xml";

static const char *GCODE_XML_TAG_SETTING = "setting";

static const char *GCODE_XML_ATTR_SETTING_VOXEL_RESOLUTION = "voxel-resolution";
static const char *GCODE_XML_ATTR_SETTING_CURVE_SEGMENTS = "curve-segments";
static const char *GCODE_XML_ATTR_SETTING_ROUGHING_OVERLAP = "roughing-overlap";
static const char *GCODE_XML_ATTR_SETTING_PADDING_FRACTION = "padding-fraction";

typedef struct gui_settings_s
{
  int voxel_resolution;
  int curve_segments;
  gfloat_t roughing_overlap;
  gfloat_t padding_fraction;
} gui_settings_t;

void gui_settings_init (gui_settings_t *settings);
void gui_settings_free (gui_settings_t *settings);
int gui_settings_read (gui_settings_t *settings);

#endif
