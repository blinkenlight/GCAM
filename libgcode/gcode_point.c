/**
 *  gcode_point.c
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

#include "gui_define.h"
#include "gcode_point.h"

void
gcode_point_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_point_t *point;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_POINT, 0);

  (*block)->free = gcode_point_free;
  (*block)->save = gcode_point_save;
  (*block)->load = gcode_point_load;
  (*block)->draw = gcode_point_draw;
  (*block)->move = gcode_point_move;
  (*block)->spin = gcode_point_spin;
  (*block)->flip = gcode_point_flip;
  (*block)->scale = gcode_point_scale;
  (*block)->parse = gcode_point_parse;
  (*block)->clone = gcode_point_clone;

  (*block)->pdata = malloc (sizeof (gcode_point_t));

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Point");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  point = (gcode_point_t *)(*block)->pdata;

  point->p[0] = 0.0;
  point->p[1] = 0.0;
}

void
gcode_point_free (gcode_block_t **block)
{
  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_point_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_point_t *point;
  uint32_t size;
  uint8_t data;

  point = (gcode_point_t *)block->pdata;

  if (block->gcode->format == GCODE_FORMAT_XML)                                 // Save to new xml format
  {
    int indent = GCODE_XML_BASE_INDENT;

    index_block = block->parent;

    while (index_block)
    {
      indent++;

      index_block = index_block->parent;
    }

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_POINT);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_2D_FLT (fh, GCODE_XML_ATTR_POINT_POSITION, point->p);
    GCODE_WRITE_XML_CL_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    GCODE_WRITE_BINARY_1X_POINT (fh, GCODE_BIN_DATA_POINT_POSITION, point->p);
  }
}

void
gcode_point_load (gcode_block_t *block, FILE *fh)
{
  gcode_point_t *point;
  uint32_t bsize, dsize, start;
  uint8_t data;

  point = (gcode_point_t *)block->pdata;

  fread (&bsize, sizeof (uint32_t), 1, fh);

  start = ftell (fh);

  while (ftell (fh) - start < bsize)
  {
    fread (&data, sizeof (uint8_t), 1, fh);
    fread (&dsize, sizeof (uint32_t), 1, fh);

    switch (data)
    {
      case GCODE_BIN_DATA_BLOCK_COMMENT:
        fread (block->comment, sizeof (char), dsize, fh);
        break;

      case GCODE_BIN_DATA_BLOCK_FLAGS:
        fread (&block->flags, sizeof (uint8_t), dsize, fh);
        break;

      case GCODE_BIN_DATA_POINT_POSITION:
        fread (point->p, sizeof (gfloat_t), 2, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_point_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_vec2d_t p;

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  gcode_point_with_offset (block, p);

  glPointSize (GCODE_OPENGL_SMALL_POINT_SIZE);
  glColor3f (GCODE_OPENGL_SMALL_POINT_COLOR[0],
             GCODE_OPENGL_SMALL_POINT_COLOR[1],
             GCODE_OPENGL_SMALL_POINT_COLOR[2]);
  glBegin (GL_POINTS);
  glVertex3f (p[0], p[1], 0.0);
  glEnd ();
#endif
}

void
gcode_point_move (gcode_block_t *block, gcode_vec2d_t delta)
{
  gcode_point_t *point;
  gcode_vec2d_t orgnl_pt, xform_pt;

  point = (gcode_point_t *)block->pdata;

  GCODE_MATH_VEC2D_COPY (orgnl_pt, point->p);
  GCODE_MATH_TRANSLATE (xform_pt, orgnl_pt, delta);
  GCODE_MATH_VEC2D_COPY (point->p, xform_pt);
}

void
gcode_point_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_point_t *point;
  gcode_vec2d_t orgnl_pt, xform_pt;

  point = (gcode_point_t *)block->pdata;

  GCODE_MATH_VEC2D_SUB (orgnl_pt, point->p, datum);
  GCODE_MATH_ROTATE (xform_pt, orgnl_pt, angle);
  GCODE_MATH_VEC2D_ADD (point->p, xform_pt, datum);
}

void
gcode_point_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_point_t *point;

  point = (gcode_point_t *)block->pdata;

  if (GCODE_MATH_IS_EQUAL (angle, 0))
  {
    point->p[1] -= datum[1];
    point->p[1] = -point->p[1];
    point->p[1] += datum[1];
  }

  if (GCODE_MATH_IS_EQUAL (angle, 90))
  {
    point->p[0] -= datum[0];
    point->p[0] = -point->p[0];
    point->p[0] += datum[0];
  }
}

void
gcode_point_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_point_t *point;

  point = (gcode_point_t *)block->pdata;

  point->p[0] *= scale;
  point->p[1] *= scale;
}

void
gcode_point_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_point_t *point;

  point = (gcode_point_t *)block->pdata;

  for (int i = 0; xmlattr[i]; i += 2)
  {
    int m;
    unsigned int n;
    double xyz[3], w;
    const char *name, *value;

    name = xmlattr[i];
    value = xmlattr[i + 1];

    if (strcmp (name, GCODE_XML_ATTR_BLOCK_COMMENT) == 0)
    {
      GCODE_PARSE_XML_ATTR_STRING (block->comment, value);
    }
    else if (strcmp (name, GCODE_XML_ATTR_BLOCK_FLAGS) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_AS_HEX (n, value))
        block->flags = n;
    }
    else if (strcmp (name, GCODE_XML_ATTR_POINT_POSITION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_FLT (xyz, value))
        for (int j = 0; j < 2; j++)
          point->p[j] = (gfloat_t)xyz[j];
    }
  }
}

void
gcode_point_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_point_t *point, *model_point;

  model_point = (gcode_point_t *)model->pdata;

  gcode_point_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  point = (gcode_point_t *)(*block)->pdata;

  point->p[0] = model_point->p[0];
  point->p[1] = model_point->p[1];
}

/**
 * Based on the point data retrieved from 'block' and the offset data referenced
 * by 'block's offset pointer, calculate a new point rotated and translated by 
 * 'offset->rotation' and 'offset->origin'.
 */

void
gcode_point_with_offset (gcode_block_t *block, gcode_vec2d_t p)
{
  gcode_point_t *point;
  gcode_vec2d_t xform_p;

  point = (gcode_point_t *)block->pdata;

  GCODE_MATH_ROTATE (xform_p, point->p, block->offset->rotation);
  GCODE_MATH_TRANSLATE (xform_p, xform_p, block->offset->origin);

  p[0] = xform_p[0];
  p[1] = xform_p[1];
}
