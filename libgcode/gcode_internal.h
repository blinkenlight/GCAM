/**
 *  gcode_internal.h
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

#ifndef _GCODE_INTERNAL_H
#define _GCODE_INTERNAL_H

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <inttypes.h>
#include "gcode_math.h"

#define NONE              0

#define GCODE_USE_OPENGL  1

#ifdef GCODE_USE_OPENGL
#include <GL/gl.h>
#endif

/**
 * Machine specific options.
 */

#define GCODE_MACHINE_OPTION_SPINDLE_CONTROL        0x01
#define GCODE_MACHINE_OPTION_AUTOMATIC_TOOL_CHANGE  0x02
#define GCODE_MACHINE_OPTION_HOME_SWITCHES          0x04
#define GCODE_MACHINE_OPTION_COOLANT                0x08

/**
 * GCODE_BIN_DATA_BLOCK_XXX flags are complimentary to the specialized DATA_BLOCK
 * ids found in the header of each block that begin with the id 0x00.
 * For this reason we count backwards from 0xff for these special #defines.
 */

#define GCODE_BIN_DATA_BLOCK_FLAGS    0xFE
#define GCODE_BIN_DATA_BLOCK_COMMENT  0xFF

#define GCODE_UNITS_INCH              0x00
#define GCODE_UNITS_MILLIMETER        0x01

#define GCODE_MATERIAL_ALUMINUM       0x00
#define GCODE_MATERIAL_FOAM           0x01
#define GCODE_MATERIAL_PLASTIC        0x02
#define GCODE_MATERIAL_STEEL          0x03
#define GCODE_MATERIAL_WOOD           0x04

#define GCODE_GET                     0x00
#define GCODE_SET                     0x01
#define GCODE_GET_WITH_OFFSET         0x02
#define GCODE_GET_NORMAL              0x03
#define GCODE_GET_TANGENT             0x04

#define GCODE_FORMAT_TBD              0x00
#define GCODE_FORMAT_BIN              0x01
#define GCODE_FORMAT_XML              0x02

#define GCODE_DRIVER_LINUXCNC         0x00
#define GCODE_DRIVER_TURBOCNC         0x01
#define GCODE_DRIVER_HAAS             0x02

#define GCODE_FLAGS_LOCK              0x01
#define GCODE_FLAGS_SUPPRESS          0x02

/* *INDENT-OFF* */

enum
{
  GCODE_TYPE_BEGIN,
  GCODE_TYPE_END,
  GCODE_TYPE_TEMPLATE,
  GCODE_TYPE_TOOL,
  GCODE_TYPE_CODE,
  GCODE_TYPE_EXTRUSION,
  GCODE_TYPE_SKETCH,
  GCODE_TYPE_LINE,
  GCODE_TYPE_ARC,
  GCODE_TYPE_BEZIER,
  GCODE_TYPE_IMAGE,
  GCODE_TYPE_BOLT_HOLES,
  GCODE_TYPE_DRILL_HOLES,
  GCODE_TYPE_POINT,
  GCODE_TYPE_STL,
  GCODE_TYPE_NUM
};

static const char *GCODE_TYPE_STRING[GCODE_TYPE_NUM] = 
{
  "BEGIN",
  "END",
  "TEMPLATE",
  "TOOL",
  "CODE",
  "EXTRUSION",
  "SKETCH",
  "LINE",
  "ARC",
  "BEZIER",
  "IMAGE",
  "BOLT HOLES",
  "DRILL HOLES",
  "POINT",
  "STL"
};

/**
 * List of validity of each block type without a parent (at top level)
 * NOTE: vaporware types ('CODE', 'BEZIER', 'STL') are all invalid now;
 * if and when they get implemented, this will have to be revised...
 */
 
static const bool GCODE_IS_VALID_IF_NO_PARENT[GCODE_TYPE_NUM] =
  {1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0};

/**
 * Matrix of validity of each block type as [parent] of [child] pairs;
 * each row is a list of valid child types for a specific parent type.
 * NOTE: vaporware types ('CODE', 'BEZIER', 'STL') are currently never
 * valid, neither as a parent nor as a child of any other block type;
 * if and when they get implemented, this will have to be revised...
 */ 
 
static const bool GCODE_IS_VALID_PARENT_CHILD[GCODE_TYPE_NUM][GCODE_TYPE_NUM] =
{ 
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

/* *INDENT-ON* */

/**
 * Various XML related constants
 * WARNING: string data saved into the XML file does get sanitized but string
 * constants DO NOT. Therefore making sure any XML strings ARE actually valid
 * sounds like a REALLY good idea.
 */

static const int GCODE_XML_BUFFER_SIZE = 0x00001000;

static const uint8_t GCODE_XML_FLAG_PROJECT = 0x01;
static const uint8_t GCODE_XML_FLAG_GCODE = 0x02;
static const uint8_t GCODE_XML_FLAG_BEGIN = 0x04;
static const uint8_t GCODE_XML_FLAG_END = 0x08;

static const uint8_t GCODE_XML_FLAGS_NEEDED = 0x0F;

static const uint8_t GCODE_XML_ATTACH_UNDER = 1;
static const uint8_t GCODE_XML_ATTACH_AFTER = 2;

static const int GCODE_XML_BASE_INDENT = 2;

static const char *GCODE_XML_FILETYPE = ".gcamx";

static const char *GCODE_XML_PROLOG = "xml version=\"1.0\" encoding=\"UTF-8\"";
static const char *GCODE_XML_FIRST_COMMENT = "===== GCAM project file =====";
static const char *GCODE_XML_SECOND_COMMENT = "created by version ";
static const char *GCODE_XML_THIRD_COMMENT = "=============================";

static const char *GCODE_XML_TAG_PROJECT = "gcam-project";
static const char *GCODE_XML_TAG_GCODE = "gcode";
static const char *GCODE_XML_TAG_BEGIN = "begin";
static const char *GCODE_XML_TAG_END = "end";
static const char *GCODE_XML_TAG_TOOL = "tool";
static const char *GCODE_XML_TAG_TEMPLATE = "template";
static const char *GCODE_XML_TAG_SKETCH = "sketch";
static const char *GCODE_XML_TAG_DRILL_HOLES = "drill-holes";
static const char *GCODE_XML_TAG_BOLT_HOLES = "bolt-holes";
static const char *GCODE_XML_TAG_EXTRUSION = "extrusion";
static const char *GCODE_XML_TAG_LINE = "line";
static const char *GCODE_XML_TAG_ARC = "arc";
static const char *GCODE_XML_TAG_POINT = "point";
static const char *GCODE_XML_TAG_IMAGE = "image";

static const char *GCODE_XML_ATTR_BLOCK_COMMENT = "comment";
static const char *GCODE_XML_ATTR_BLOCK_FLAGS = "flags";

/**
 * Pre-declaration of circular-referenced structs
 */

struct gcode_s;
struct gcode_block_s;

/**
 * Type definitions for block-specific functions
 */

typedef void gcode_free_t (struct gcode_block_s **block);
typedef void gcode_save_t (struct gcode_block_s *block, FILE *fh);
typedef void gcode_load_t (struct gcode_block_s *block, FILE *fh);
typedef void gcode_make_t (struct gcode_block_s *block);
typedef void gcode_draw_t (struct gcode_block_s *block, struct gcode_block_s *selected);
typedef int gcode_eval_t (struct gcode_block_s *block, gfloat_t y, gfloat_t *x_array, uint32_t *xind);
typedef int gcode_ends_t (struct gcode_block_s *block, gcode_vec2d_t p0, gcode_vec2d_t p1, uint8_t mode);
typedef void gcode_aabb_t (struct gcode_block_s *block, gcode_vec2d_t min, gcode_vec2d_t max);
typedef gfloat_t gcode_length_t (struct gcode_block_s *block);
typedef void gcode_move_t (struct gcode_block_s *block, gcode_vec2d_t delta);
typedef void gcode_spin_t (struct gcode_block_s *block, gcode_vec2d_t datum, gfloat_t angle);
typedef void gcode_scale_t (struct gcode_block_s *block, gfloat_t scale);
typedef void gcode_parse_t (struct gcode_block_s *block, const char **xmlattr);
typedef void gcode_clone_t (struct gcode_block_s **block, struct gcode_s *gcode, struct gcode_block_s *model);

typedef void gcode_progress_callback_t (void *gui, gfloat_t progress);
typedef void gcode_message_callback_t (void *gui, char *message);

/**
 * Type definitions for commonly used structs
 */

typedef struct gcode_offset_s
{
  gfloat_t side;
  gfloat_t tool;
  gfloat_t eval;
  gfloat_t rotation;
  gcode_vec2d_t origin;
  gcode_vec2d_t z;                                                              // Z values for helical paths
} gcode_offset_t;

typedef struct gcode_block_s
{
  uint8_t type;
  uint8_t flags;                                                                // Flags include: lock, suppress

  char comment[64];
  char status[64];

  uint32_t name;                                                                // This is used primarily for opengl picking, so that this blocks rendered lines link back to something in the treeview

  struct gcode_s *gcode;

  struct gcode_block_s *parent;
  struct gcode_block_s *listhead;
  struct gcode_block_s *extruder;
  struct gcode_block_s *prev;
  struct gcode_block_s *next;

  gcode_offset_t *offref;
  gcode_offset_t *offset;

  void *pdata;

  int code_alloc;
  int code_len;
  char *code;

  gcode_free_t *free;
  gcode_save_t *save;
  gcode_load_t *load;
  gcode_make_t *make;
  gcode_draw_t *draw;
  gcode_eval_t *eval;
  gcode_ends_t *ends;
  gcode_aabb_t *aabb;
  gcode_length_t *length;
  gcode_move_t *move;
  gcode_spin_t *spin;
  gcode_scale_t *scale;
  gcode_parse_t *parse;
  gcode_clone_t *clone;
} gcode_block_t;

typedef struct gcode_s
{
  char name[32];
  char notes[512];

  uint8_t units;
  uint8_t material_type;

  gfloat_t material_size[3];
  gfloat_t material_origin[3];
  gfloat_t ztraverse;

  void *gui;

  gcode_block_t *listhead;

  gcode_progress_callback_t *progress_callback;
  gcode_message_callback_t *message_callback;

  gcode_offset_t zero_offset;

  uint16_t voxel_resolution;
  uint16_t voxel_number[3];
  uint8_t *voxel_map;

  gfloat_t tool_xpos;
  gfloat_t tool_ypos;
  gfloat_t tool_zpos;

  uint8_t format;
  uint8_t driver;

  char machine_name[32];

  uint8_t machine_options;

  uint32_t decimals;                                                            // Number of decimal places to print

  uint32_t project_number;                                                      // For Haas Machines only
} gcode_t;

typedef struct xml_context_s
{
  gcode_t *gcode;
  gcode_block_t *block;
  uint8_t state;
  uint8_t modus;
  uint8_t error;
  int32_t index;
  int32_t limit;
  int32_t chars;
  char cache[32];
} xml_context_t;

/**
 * Function prototypes
 */

void gcode_internal_init (gcode_block_t *block, gcode_t *gcode, gcode_block_t *parent, uint8_t type, uint8_t flags);
void gsprintf (char *target, unsigned int number, char *format, ...);
void strswp (char *target, char oldchar, char newchar);

/**
 * Miscellaneous macros
 */

#define REMARK(...) { \
        fprintf (stderr, "Error in '%s()' near line %d:\n", __func__, __LINE__); \
        fprintf (stderr, ## __VA_ARGS__); }

/**
 * Scale imperial defaults to relatively similar but cleanly rounded metric values if needed.
 * NOTE: the input value must ALWAYS be in INCHES (eg. numeric constants for default values);
 * One should NEVER USE THIS on variables already scaled to metric units in metric projects.
 */

#define EQUIV_UNITS(_unit, _num) \
        ((_unit == GCODE_UNITS_MILLIMETER) ? _num * 25.0 : _num)

/**
 * Same as above, but values are scaled (if needed) to the units currently used by "_gcode".
 * NOTE: the input value must ALWAYS be in INCHES (eg. numeric constants for default values);
 * One should NEVER USE THIS on variables already scaled to metric units in metric projects.
 */

#define GCODE_UNITS(_gcode, _num) \
        EQUIV_UNITS(_gcode->units, _num)

#define GCODE_INIT(_block) { \
        _block->code = NULL; }

#define GCODE_CLEAR(_block) { \
        _block->code_len = 1; \
        _block->code_alloc = 1; \
        _block->code = realloc (_block->code, _block->code_alloc); \
        _block->code[0] = '\0'; }

/**
 * increase by 64kB + string length + 1 if request to append greater than current buffer size.
 * includes optimized manual strcat code to vastly improve performance for large strings.
 */

#define GCODE_APPEND(_block, _str) { \
        int _slen = strlen (_str) + 1, _i; \
        if (_block->code_len + _slen > _block->code_alloc) \
        { \
          _block->code_alloc +=  (1 << 16) + _slen; \
          _block->code = realloc (_block->code, _block->code_alloc); \
        } \
        for (_i = 0; _i < _slen; _i++) \
          _block->code[_block->code_len + _i - 1] = _str[_i]; \
        _block->code_len += _slen - 1; }

#define GCODE_NEWLINE(_block) { \
        GCODE_APPEND (_block, "\n"); }

#define GCODE_PADDING(_block, _comment) { \
        if (*_comment) \
          GCODE_APPEND (_block, " "); }

#define GCODE_COMMENT(_block, _comment) { \
        char _string[256]; \
        if (*_comment) \
        { \
          if (_block->gcode->driver == GCODE_DRIVER_TURBOCNC) \
            sprintf (_string, "; %s\n", _comment); \
          else \
            sprintf (_string, "(%s)\n", _comment); \
        } \
        else \
        { \
          sprintf (_string, "\n"); \
        } \
        GCODE_APPEND (_block, _string); }

#define GCODE_COMMAND(_block, _command, _comment) { \
        char _string[256]; \
        sprintf (_string, "%s ", _command); \
        GCODE_APPEND (_block, _string); \
        GCODE_COMMENT (_block, _comment); }

/* Feed and speed macros */

#define GCODE_F_VALUE(_block, _feed, _comment) { \
        char _string[256]; \
        sprintf (_string, "F%.3f", _feed); \
        GCODE_APPEND (_block, _string); \
        GCODE_PADDING (_block, _comment); \
        GCODE_COMMENT (_block, _comment); }

#define GCODE_S_VALUE(_block, _speed, _comment) { \
        char _string[256]; \
        sprintf (_string, "S%d", _speed); \
        GCODE_APPEND (_block, _string); \
        GCODE_PADDING (_block, _comment); \
        GCODE_COMMENT (_block, _comment); }

/* Plunge and retract macros - WARNING: THESE WORK RELATIVE TO THE Z-ORIGIN, WHATEVER THAT MEANS... */

#define GCODE_DESCEND(_block, _depth, _tool) { \
        gfloat_t _z = _block->gcode->material_origin[2] + _depth; \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_zpos, _z)) \
        { \
          char _string[256]; \
          gsprintf (_string, _block->gcode->decimals, "G01 Z%z F%.3f ", _z, _tool->feed * _tool->plunge_ratio); \
          GCODE_APPEND (_block, _string); \
          GCODE_COMMENT (_block, "slow plunge"); \
          sprintf (_string, "F%.3f ", _tool->feed); \
          GCODE_APPEND (_block, _string); \
          GCODE_COMMENT (_block, "restore feed rate"); \
          _block->gcode->tool_zpos = _z; \
        }}

#define GCODE_PLUMMET(_block, _depth) { \
        gfloat_t _z = _block->gcode->material_origin[2] + _depth; \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_zpos, _z)) \
        { \
          char _string[256]; \
          gsprintf (_string, _block->gcode->decimals, "G00 Z%z ", _z); \
          GCODE_APPEND (_block, _string); \
          GCODE_COMMENT (_block, "fast plunge"); \
          _block->gcode->tool_zpos = _z; \
        }}

#define GCODE_RETRACT(_block, _depth) { \
        gfloat_t _z = _block->gcode->material_origin[2] + _depth; \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_zpos, _z)) \
        { \
          char _string[256]; \
          gsprintf (_string, _block->gcode->decimals, "G00 Z%z ", _z); \
          GCODE_APPEND (_block, _string); \
          GCODE_COMMENT (_block, "retract"); \
          _block->gcode->tool_zpos = _z; \
        }}

#define GCODE_PULL_UP(_block, _depth) { \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_zpos, _depth)) \
        { \
          char _string[256]; \
          gsprintf (_string, _block->gcode->decimals, "G00 Z%z ", _depth); \
          GCODE_APPEND (_block, _string); \
          GCODE_COMMENT (_block, "retract"); \
          _block->gcode->tool_zpos = _depth; \
        }}

/* Line and arc movement macros */

#define GCODE_XY_PAIR(_block, _x, _y, _comment) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "X%z Y%z", _x, _y); \
        GCODE_APPEND (_block, _string); \
        GCODE_PADDING (_block, _comment); \
        GCODE_COMMENT (_block, _comment); \
        _block->gcode->tool_xpos = _x; \
        _block->gcode->tool_ypos = _y; }

#define GCODE_2D_MOVE(_block, _x, _y, _comment) { \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_xpos, _x) || \
            !GCODE_MATH_IS_EQUAL (_block->gcode->tool_ypos, _y)) \
        { \
          char _string[256]; \
          GCODE_APPEND (_block, "G00"); \
          if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_xpos, _x)) \
          { \
            gsprintf (_string, _block->gcode->decimals, " X%z", _x); \
            GCODE_APPEND (_block, _string); \
          } \
          if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_ypos, _y)) \
          { \
            gsprintf (_string, _block->gcode->decimals, " Y%z", _y); \
            GCODE_APPEND (_block, _string); \
          } \
          GCODE_PADDING (_block, _comment); \
          GCODE_COMMENT (_block, _comment); \
          _block->gcode->tool_xpos = _x; \
          _block->gcode->tool_ypos = _y; \
        }}

#define GCODE_2D_LINE(_block, _x, _y, _comment) { \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_xpos, _x) || \
            !GCODE_MATH_IS_EQUAL (_block->gcode->tool_ypos, _y)) \
        { \
          char _string[256]; \
          GCODE_APPEND (_block, "G01"); \
          if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_xpos, _x)) \
          { \
            gsprintf (_string, _block->gcode->decimals, " X%z", _x); \
            GCODE_APPEND (_block, _string); \
          } \
          if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_ypos, _y)) \
          { \
            gsprintf (_string, _block->gcode->decimals, " Y%z", _y); \
            GCODE_APPEND (_block, _string); \
          } \
          GCODE_PADDING (_block, _comment); \
          GCODE_COMMENT (_block, _comment); \
          _block->gcode->tool_xpos = _x; \
          _block->gcode->tool_ypos = _y; \
        }}

#define GCODE_3D_LINE(_block, _x, _y, _z, _comment) { \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_xpos, _x) || \
            !GCODE_MATH_IS_EQUAL (_block->gcode->tool_ypos, _y) || \
            !GCODE_MATH_IS_EQUAL (_block->gcode->tool_zpos, _z)) \
        { \
          char _string[256]; \
          GCODE_APPEND (_block, "G01"); \
          if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_xpos, _x)) \
          { \
            gsprintf (_string, _block->gcode->decimals, " X%z", _x); \
            GCODE_APPEND (_block, _string); \
          } \
          if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_ypos, _y)) \
          { \
            gsprintf (_string, _block->gcode->decimals, " Y%z", _y); \
            GCODE_APPEND (_block, _string); \
          } \
          if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_zpos, _z)) \
          { \
            gsprintf (_string, _block->gcode->decimals, " Z%z", _z); \
            GCODE_APPEND (_block, _string); \
          } \
          GCODE_PADDING (_block, _comment); \
          GCODE_COMMENT (_block, _comment); \
          _block->gcode->tool_xpos = _x; \
          _block->gcode->tool_ypos = _y; \
          _block->gcode->tool_zpos = _z; \
        }}

#define GCODE_2D_ARC_CW(_block, _x, _y, _i, _j, _comment) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "G02 X%z Y%z I%z J%z ", _x, _y, _i, _j); \
        GCODE_APPEND (_block, _string); \
        GCODE_COMMENT (_block, _comment); \
        _block->gcode->tool_xpos = _x; \
        _block->gcode->tool_ypos = _y; }

#define GCODE_3D_ARC_CW(_block, _x, _y, _z, _i, _j, _comment) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "G02 X%z Y%z Z%z I%z J%z ", _x, _y, _z, _i, _j); \
        GCODE_APPEND (_block, _string); \
        GCODE_COMMENT (_block, _comment); \
        _block->gcode->tool_xpos = _x; \
        _block->gcode->tool_ypos = _y; \
        _block->gcode->tool_zpos = _z; }

#define GCODE_2D_ARC_CCW(_block, _x, _y, _i, _j, _comment) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "G03 X%z Y%z I%z J%z ", _x, _y, _i, _j); \
        GCODE_APPEND (_block, _string); \
        GCODE_COMMENT (_block, _comment); \
        _block->gcode->tool_xpos = _x; \
        _block->gcode->tool_ypos = _y; }

#define GCODE_3D_ARC_CCW(_block, _x, _y, _z, _i, _j, _comment) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "G03 X%z Y%z Z%z I%z J%z ", _x, _y, _z, _i, _j); \
        GCODE_APPEND (_block, _string); \
        GCODE_COMMENT (_block, _comment); \
        _block->gcode->tool_xpos = _x; \
        _block->gcode->tool_ypos = _y; \
        _block->gcode->tool_zpos = _z; }

/* Canned cycle macros */

#define GCODE_DRILL(_block, _code, _z, _f, _r) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "%s Z%z F%.3f R%z ", _code, _z, _f, _r); \
        GCODE_APPEND (_block, _string); \
        _block->gcode->tool_zpos = FLT_MAX; }

#define GCODE_Q_DRILL(_block, _code, _z, _f, _r, _q) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "%s Z%z F%.3f R%z Q%z ", _code, _z, _f, _r, _q); \
        GCODE_APPEND (_block, _string); \
        _block->gcode->tool_zpos = FLT_MAX; }

/* Other command macros */

#define GCODE_GO_HOME(_block, _depth) { \
        char _string[256]; \
        gsprintf (_string, _block->gcode->decimals, "G28 Z%z ", (_block->gcode->material_origin[2] + _depth)); \
        GCODE_APPEND (_block, _string); \
        GCODE_COMMENT (_block, "return to home"); \
        _block->gcode->tool_xpos = FLT_MAX; \
        _block->gcode->tool_ypos = FLT_MAX; \
        _block->gcode->tool_zpos = FLT_MAX; }

#define GCODE_MOVE_TO(_block, _x, _y, _z, _travel_z, _touch_z, _tool, _target) { \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_xpos, _x) || \
            !GCODE_MATH_IS_EQUAL (_block->gcode->tool_ypos, _y)) \
        { \
          char _remark[256]; \
          sprintf (_remark, "move to %s", _target); \
          GCODE_RETRACT (_block, _travel_z); \
          GCODE_2D_MOVE (_block, _x, _y, _remark); \
        } \
        if (!GCODE_MATH_IS_EQUAL (_block->gcode->tool_zpos, _z)) \
        { \
          if (_touch_z >= _z) \
          { \
            GCODE_PLUMMET (_block, _touch_z); \
          } \
          GCODE_DESCEND (_block, _z, _tool); \
        }}

/**
 * Macros for writing the binary type savefile
 */

#define GCODE_WRITE_BINARY_NUM_DATA(_fh, _desc, _size, _ptr) { \
        uint8_t _d = _desc; \
        uint32_t _s = _size; \
        fwrite (&_d, sizeof (uint8_t), 1, _fh); \
        fwrite (&_s, sizeof (uint32_t), 1, _fh); \
        fwrite (_ptr, _size, 1, _fh); }

#define GCODE_WRITE_BINARY_STR_DATA(_fh, _desc, _ptr) { \
        uint8_t _d = _desc; \
        uint32_t _s = strlen (_ptr) + 1; \
        fwrite (&_d, sizeof (uint8_t), 1, _fh); \
        fwrite (&_s, sizeof (uint32_t), 1, _fh); \
        fwrite (_ptr, sizeof (char), _s, _fh); }

#define GCODE_WRITE_BINARY_1X_POINT(_fh, _desc, _ptr) { \
        uint8_t _d = _desc; \
        uint32_t _s = 2 * sizeof (gfloat_t); \
        fwrite (&_d, sizeof (uint8_t), 1, _fh); \
        fwrite (&_s, sizeof (uint32_t), 1, _fh); \
        fwrite (_ptr, sizeof (gfloat_t), 2, _fh); }

#define GCODE_WRITE_BINARY_2X_POINT(_fh, _desc, _ptr_1, _ptr_2) { \
        uint8_t _d = _desc; \
        uint32_t _s = 4 * sizeof (gfloat_t); \
        fwrite (&_d, sizeof (uint8_t), 1, _fh); \
        fwrite (&_s, sizeof (uint32_t), 1, _fh); \
        fwrite (_ptr_1, sizeof (gfloat_t), 2, _fh); \
        fwrite (_ptr_2, sizeof (gfloat_t), 2, _fh); }

#define GCODE_WRITE_BINARY_2D_ARRAY(_fh, _desc, _res_x, _res_y, _ptr) { \
        uint8_t _d = _desc; \
        uint32_t _s = _res_x * _res_y * sizeof (gfloat_t); \
        fwrite (&_d, sizeof (uint8_t), 1, _fh); \
        fwrite (&_s, sizeof (uint32_t), 1, _fh); \
        fwrite (_ptr, sizeof (gfloat_t), _res_x * _res_y, _fh); }

/**
 * Macros for writing the XML type savefile
 */

#define GCODE_WRITE_XML_INDENT_TABS(_file, _indent) { \
        for (int _i=0; _i<_indent; _i++) \
          fprintf (_file, "\t"); }

#define GCODE_WRITE_XML_PROLOG_LINE(_file, _prolog) { \
        fprintf (_file, "<?%s?>", _prolog); }

#define GCODE_WRITE_XML_REMARK_LINE(_file, _comment) { \
        fprintf (_file, "<!-- %s -->", _comment); }

#define GCODE_WRITE_XML_REMARK_JOIN(_file, _comment1, _comment2) { \
        fprintf (_file, "<!-- %s%s -->", _comment1, _comment2); }

#define GCODE_WRITE_XML_HEAD_OF_TAG(_file, _tag) { \
        fprintf (_file, "<%s", _tag); }

#define GCODE_WRITE_XML_OP_TAG_TAIL(_file) { \
        fprintf (_file, ">"); }

#define GCODE_WRITE_XML_CL_TAG_TAIL(_file) { \
        fprintf (_file, " />"); }

#define GCODE_WRITE_XML_END_TAG_FOR(_file, _tag) { \
        fprintf (_file, "</%s>", _tag); }

#define GCODE_WRITE_XML_ATTR_STRING(_file, _name, _value) { \
        char *_safestr = malloc (gcode_util_xml_safelen (_value)); \
        gcode_util_xml_cpysafe (_safestr, _value); \
        fprintf (_file, " %s=\"%s\"", _name, _safestr); \
        free (_safestr); }

#define GCODE_WRITE_XML_ATTR_1D_INT(_file, _name, _value) { \
        fprintf (_file, " %s=\"%i\"", _name, _value); }

#define GCODE_WRITE_XML_ATTR_2D_INT(_file, _name, _value) { \
        fprintf (_file, " %s=\"%i %i\"", _name, _value[0], _value[1]); }

#define GCODE_WRITE_XML_ATTR_AS_HEX(_file, _name, _value) { \
        fprintf (_file, " %s=\"%X\"", _name, _value); }

#define GCODE_WRITE_XML_ATTR_1D_FLT(_file, _name, _value) { \
        fprintf (_file, " %s=\"%f\"", _name, _value); }

#define GCODE_WRITE_XML_ATTR_2D_FLT(_file, _name, _value) { \
        fprintf (_file, " %s=\"%f %f\"", _name, _value[0], _value[1]); }

#define GCODE_WRITE_XML_ATTR_3D_FLT(_file, _name, _value) { \
        fprintf (_file, " %s=\"%f %f %f\"", _name, _value[0], _value[1], _value[2]); }

#define GCODE_WRITE_XML_CONTENT_FLT(_file, _value) { \
        fprintf (_file, "%f ", _value); }

#define GCODE_WRITE_XML_END_OF_LINE(_file) { \
        fprintf (_file, "\n"); }

/**
 * Macros for parsing the XML type savefile
 */

#define GCODE_PARSE_XML_ATTR_STRING(_clone, _string) { \
        strncpy (_clone, _string, sizeof(_clone)); \
        _clone[sizeof(_clone) - 1] = '\0'; }

#define GCODE_PARSE_XML_ATTR_1D_INT(_value, _string) \
        (sscanf (_string, "%i", &_value) == 1)

#define GCODE_PARSE_XML_ATTR_2D_INT(_value, _string) \
        (sscanf (_string, "%i %i", &_value[0], &_value[1]) == 2)

#define GCODE_PARSE_XML_ATTR_AS_HEX(_value, _string) \
        (sscanf (_string, "%X", &_value) == 1)

#define GCODE_PARSE_XML_ATTR_1D_FLT(_value, _string) \
        (sscanf (_string, "%lf", &_value) == 1)

#define GCODE_PARSE_XML_ATTR_2D_FLT(_value, _string) \
        (sscanf (_string, "%lf %lf", &_value[0], &_value[1]) == 2)

#define GCODE_PARSE_XML_ATTR_3D_FLT(_value, _string) \
        (sscanf (_string, "%lf %lf %lf", &_value[0], &_value[1], &_value[2]) == 3)

#endif
