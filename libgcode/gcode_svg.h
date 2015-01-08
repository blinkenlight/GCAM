/**
 *  gcode_svg.h
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

#ifndef __GCODE_SVG_H_
#define __GCODE_SVG_H_

/**
 * SVG file import parser
 * Reads an SVG format XML file, extracts path data, and creates
 * line segments/arcs as appropriate (currently only line
 * segments are supported and curves are split into lines.)
 *
 * ONLY paths are supported, so objects must be converted to paths
 * to be processed
 *
 * Future possibilities:
 * -Store layer data
 * -Arc support
 */

#include "gcode.h"
#include <glib.h>

typedef struct gcode_svg_s
{
  gcode_t *gcode;
  gcode_block_t *parent_block;
  gcode_block_t *sketch_block;
  gfloat_t size[2];
  gfloat_t scale[2];
} gcode_svg_t;

typedef struct gcode_arc_by_points_s
{
  double pt0[2];
  double pt1[2];
  double rx;
  double ry;
  double phi;
  int fla;
  int fls;
} gcode_arc_by_points_t;

typedef struct gcode_arc_by_center_s
{
  double cpt[2];
  double rx;
  double ry;
  double phi;
  double theta;
  double sweep;
} gcode_arc_by_center_t;

static const char *SVG_XML_TAG_SVG = "svg";
static const char *SVG_XML_TAG_PATH = "path";

static const char *SVG_XML_ATTR_WIDTH = "width";
static const char *SVG_XML_ATTR_HEIGHT = "height";
static const char *SVG_XML_ATTR_PATH_DATA = "d";

static const char *SVG_XML_UNIT_MM = "mm";
static const char *SVG_XML_UNIT_CM = "cm";
static const char *SVG_XML_UNIT_INCH = "in";
static const char *SVG_XML_UNIT_PERCENT = "%";

static const char *SVG_NUMERIC_CHARS = "0123456789+-Ee.";
static const char *SVG_PATH_COMMANDS = "MmLlHhVvAaQqTtCcSsZz";

int gcode_svg_import (gcode_block_t *template_block, char *filename);

#define SVG_PARSE_1X_VALUE(_value, _string) \
        (sscanf (_string, "%lf", &_value) == 1)

#define SVG_PARSE_1X_POINT(_point, _string) \
        (sscanf (_string, "%lf %lf", &_point[0], &_point[1]) == 2)

#define SVG_PARSE_2X_POINT(_point1, _point2, _string) \
        (sscanf (_string, "%lf %lf %lf %lf", &_point1[0], &_point1[1], &_point2[0], &_point2[1]) == 4)

#define SVG_PARSE_3X_POINT(_point1, _point2, _point3, _string) \
        (sscanf (_string, "%lf %lf %lf %lf %lf %lf", &_point1[0], &_point1[1], &_point2[0], &_point2[1], &_point3[0], &_point3[1]) == 6)

#define SVG_PARSE_ARC_DATA(_value1, _value2, _value3, _flag1, _flag2, _point, _string) \
        (sscanf (_string, "%lf %lf %lf %i %i %lf %lf", &_value1, &_value2, &_value3, &_flag1, &_flag2, &_point[0], &_point[1]) == 7)

#endif
