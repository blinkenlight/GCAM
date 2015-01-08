/**
 *  gcode.h
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

#ifndef _GCODE_H
#define _GCODE_H

#include "gcode_internal.h"

#define GCODE_BIN_FILE_HEADER           0x4743414d
#define GCODE_VERSION                   0x20100727

#define GCODE_BIN_DATA                  0xff
#define GCODE_BIN_DATA_NAME             0x01
#define GCODE_BIN_DATA_UNITS            0x02
#define GCODE_BIN_DATA_MATERIAL_TYPE    0x03
#define GCODE_BIN_DATA_MATERIAL_SIZE    0x04
#define GCODE_BIN_DATA_ZTRAVERSE        0x05
#define GCODE_BIN_DATA_NOTES            0x06
#define GCODE_BIN_DATA_MATERIAL_ORIGIN  0x07

#define GCODE_BIN_DATA_MACHINE          0xfe
#define GCODE_BIN_DATA_MACHINE_NAME     0x01
#define GCODE_BIN_DATA_MACHINE_OPTIONS  0x02

static const char *GCODE_XML_ATTR_PROJECT_VERSION = "version";

static const char *GCODE_XML_ATTR_GCODE_NAME = "name";
static const char *GCODE_XML_ATTR_GCODE_UNITS = "units";
static const char *GCODE_XML_ATTR_GCODE_MATERIAL_TYPE = "material-type";
static const char *GCODE_XML_ATTR_GCODE_MATERIAL_SIZE = "material-size";
static const char *GCODE_XML_ATTR_GCODE_MATERIAL_ORIGIN = "material-origin";
static const char *GCODE_XML_ATTR_GCODE_Z_TRAVERSE = "z-traverse";
static const char *GCODE_XML_ATTR_GCODE_NOTES = "notes";
static const char *GCODE_XML_ATTR_GCODE_MACHINE_NAME = "machine-name";
static const char *GCODE_XML_ATTR_GCODE_MACHINE_OPTIONS = "machine-options";

void gcode_attach_as_extruder (gcode_block_t *sel_block, gcode_block_t *block);
void gcode_insert_as_listhead (gcode_block_t *sel_block, gcode_block_t *block);
void gcode_append_as_listtail (gcode_block_t *sel_block, gcode_block_t *block);
void gcode_insert_after_block (gcode_block_t *sel_block, gcode_block_t *block);
void gcode_place_block_before (gcode_block_t *sel_block, gcode_block_t *block);
void gcode_place_block_behind (gcode_block_t *sel_block, gcode_block_t *block);
void gcode_splice_list_around (gcode_block_t *block);
void gcode_remove_and_destroy (gcode_block_t *block);

void gcode_get_furthest_prev (gcode_block_t **block);
void gcode_get_furthest_next (gcode_block_t **block);
void gcode_get_circular_prev (gcode_block_t **block);
void gcode_get_circular_next (gcode_block_t **block);

void gcode_list_insert (gcode_block_t **list, gcode_block_t *block);
void gcode_list_make (gcode_t *gcode);
void gcode_list_free (gcode_block_t **list);

void gcode_init (gcode_t *gcode);
void gcode_prep (gcode_t *gcode);
void gcode_free (gcode_t *gcode);

int gcode_save (gcode_t *gcode, const char *filename);
int gcode_load (gcode_t *gcode, const char *filename);
int gcode_export (gcode_t *gcode, const char *filename);

void gcode_render_final (gcode_t *gcode, gfloat_t *time_elapsed);

void gcode_dump_tree (gcode_t *gcode, gcode_block_t *block);

#include "gcode_begin.h"
#include "gcode_end.h"
#include "gcode_template.h"
#include "gcode_tool.h"
#include "gcode_code.h"
#include "gcode_extrusion.h"
#include "gcode_sketch.h"
#include "gcode_line.h"
#include "gcode_arc.h"
#include "gcode_bolt_holes.h"
#include "gcode_drill_holes.h"
#include "gcode_point.h"
#include "gcode_gerber.h"
#include "gcode_excellon.h"
#include "gcode_util.h"
#include "gcode_svg.h"
#include "gcode_image.h"
#include "gcode_stl.h"

#endif
