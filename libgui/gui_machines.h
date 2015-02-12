/**
 *  gui_machines.h
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

#ifndef _GUI_MACHINES_H
#define _GUI_MACHINES_H

#include "gcode.h"

static const char *GCODE_XML_MACHINES_FILENAME = "machines.xml";

static const char *GCODE_XML_TAG_MACHINE = "machine";
static const char *GCODE_XML_TAG_MACHINE_SETTING = "setting";
static const char *GCODE_XML_TAG_MACHINE_PROPERTY = "property";

static const char *GCODE_XML_ATTR_MACHINE_NAME = "name";
static const char *GCODE_XML_ATTR_PROPERTY_TRAVEL_X = "travel-x";
static const char *GCODE_XML_ATTR_PROPERTY_TRAVEL_Y = "travel-y";
static const char *GCODE_XML_ATTR_PROPERTY_TRAVEL_Z = "travel-z";
static const char *GCODE_XML_ATTR_PROPERTY_MAX_IPM_X = "max-ipm-x";
static const char *GCODE_XML_ATTR_PROPERTY_MAX_IPM_Y = "max-ipm-y";
static const char *GCODE_XML_ATTR_PROPERTY_MAX_IPM_Z = "max-ipm-z";
static const char *GCODE_XML_ATTR_PROPERTY_SPINDLE_CONTROL = "spindle-control";
static const char *GCODE_XML_ATTR_PROPERTY_TOOL_CHANGE = "tool-change";
static const char *GCODE_XML_ATTR_PROPERTY_HOME_SWITCHES = "home-switches";
static const char *GCODE_XML_ATTR_PROPERTY_COOLANT = "coolant";

static const char *GCODE_XML_VAL_PROPERTY_YES = "yes";
static const char *GCODE_XML_VAL_PROPERTY_AUTO = "auto";

typedef struct gui_machine_s
{
  char name[64];
  gfloat_t travel[3];
  gfloat_t maxipm[3];
  unsigned char options;
} gui_machine_t;

typedef struct gui_machine_list_s
{
  gui_machine_t *machine;
  int number;
} gui_machine_list_t;

void gui_machines_init (gui_machine_list_t *machine_list);
void gui_machines_free (gui_machine_list_t *machine_list);
int gui_machines_read (gui_machine_list_t *machine_list);
gui_machine_t *gui_machines_find (gui_machine_list_t *machine_list, char *machine_name, uint8_t fallback);

#endif
