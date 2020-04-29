/**
 *  gcode_line.c
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
#include "gcode_line.h"
#include "gcode.h"

void
gcode_line_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_line_t *line;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_LINE, 0);

  (*block)->free = gcode_line_free;
  (*block)->save = gcode_line_save;
  (*block)->load = gcode_line_load;
  (*block)->make = gcode_line_make;
  (*block)->draw = gcode_line_draw;
  (*block)->eval = gcode_line_eval;
  (*block)->ends = gcode_line_ends;
  (*block)->aabb = gcode_line_aabb;
  (*block)->length = gcode_line_length;
  (*block)->move = gcode_line_move;
  (*block)->spin = gcode_line_spin;
  (*block)->flip = gcode_line_flip;
  (*block)->scale = gcode_line_scale;
  (*block)->parse = gcode_line_parse;
  (*block)->clone = gcode_line_clone;

  (*block)->pdata = malloc (sizeof (gcode_line_t));

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Line");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  line = (gcode_line_t *)(*block)->pdata;

  line->p0[0] = 0.0;
  line->p0[1] = 0.0;
  line->p1[0] = GCODE_UNITS ((*block)->gcode, 1.0);
  line->p1[1] = 0.0;
}

void
gcode_line_free (gcode_block_t **block)
{
  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_line_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_line_t *line;
  uint32_t size;
  uint8_t data;

  line = (gcode_line_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_LINE);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_2D_FLT (fh, GCODE_XML_ATTR_LINE_START_POINT, line->p0);
    GCODE_WRITE_XML_ATTR_2D_FLT (fh, GCODE_XML_ATTR_LINE_END_POINT, line->p1);
    GCODE_WRITE_XML_CL_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    GCODE_WRITE_BINARY_2X_POINT (fh, GCODE_BIN_DATA_LINE_POINTS, line->p0, line->p1);
  }
}

void
gcode_line_load (gcode_block_t *block, FILE *fh)
{
  gcode_line_t *line;
  uint32_t bsize, dsize, start;
  uint8_t data;

  line = (gcode_line_t *)block->pdata;

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

      case GCODE_BIN_DATA_LINE_POINTS:
        fread (line->p0, sizeof (gfloat_t), 2, fh);
        fread (line->p1, sizeof (gfloat_t), 2, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_line_make (gcode_block_t *block)
{
  char string[256];
  gcode_vec2d_t p0, p1, normal;

  GCODE_CLEAR (block);

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  sprintf (string, "LINE: %s", block->comment);

  gcode_line_with_offset (block, p0, p1, normal);

  GCODE_2D_LINE (block, p0[0], p0[1], "");

  if (fabs (block->offset->z[0] - block->offset->z[1]) < GCODE_PRECISION)
  {
    GCODE_2D_LINE (block, p1[0], p1[1], string);
  }
  else
  {
    GCODE_3D_LINE (block, p1[0], p1[1], block->offset->z[1], string);
  }
}

void
gcode_line_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_block_t *other_block;
  gcode_vec2d_t e0, e1, p0, p1, normal;
  gfloat_t coef;
  uint32_t sindex, edited, picked;

  if (block->flags & GCODE_FLAGS_SUPPRESS)                                      // Do not draw the block if it's suppressed;
    return;

  sindex = picked = edited = 0;

  if (selected)                                                                 // If a selected block does exists, test some selection-related flags;
  {                                                                             // Coincidence is tested by "name" to allow for "snapshot" work copies.
    if (block->parent == selected)                                              // 'selected' is the parent of 'block' : highlight this block;
      sindex = 1;
    else if (block->name == selected->name)                                     // 'selected' is the same as 'block' - this needs to be "else" since ALL bolt
      sindex = picked = edited = 1;                                             // holes are named the same as their parent to help looking it up when clicked;
    else if (block->parent == selected->parent)                                 // 'selected' is in the same list as 'block' : draw endpoint markers;
      edited = 1;
  }

  if (picked)                                                                   // This is not exactly elegant, but since the "snapshot" work copy used here
  {                                                                             // could have been flipped during continuity detection therefore potentially
    gcode_line_with_offset (selected, p0, p1, normal);                          // no longer reflects original segment direction, we need to take our data
  }                                                                             // from somewhere else if direction is important. Luckily, the only such case
  else                                                                          // (drawing the currently selected block with a start-to-end gradient stroke)
  {                                                                             // can be handled fairly simply considering we also happen to have a reference
    gcode_line_with_offset (block, p0, p1, normal);                             // to the selected block in its original, untampered form: we just use that
  }                                                                             // as the data source if the current block happens to be the selected one...

  coef = picked ? 0.5 : 1.0;

  glLoadName ((GLuint) block->name);                                            // Attach the block's "name" to the line being drawn for reverse lookup;
  glLineWidth (1);

  glBegin (GL_LINES);
  glColor3f (coef * GCODE_OPENGL_SELECTABLE_COLORS[sindex][0],
             coef * GCODE_OPENGL_SELECTABLE_COLORS[sindex][1],
             coef * GCODE_OPENGL_SELECTABLE_COLORS[sindex][2]);
  glVertex3f (p0[0], p0[1], block->offset->z[0]);

  glColor3f (GCODE_OPENGL_SELECTABLE_COLORS[sindex][0],
             GCODE_OPENGL_SELECTABLE_COLORS[sindex][1],
             GCODE_OPENGL_SELECTABLE_COLORS[sindex][2]);
  glVertex3f (p1[0], p1[1], block->offset->z[1]);
  glEnd ();                                                                     // The line itself is drawn now but we might still need to draw end markers;

  if (edited)                                                                   // So if end markers are needed...
  {
    if (picked)                                                                 // ...they're either standard "selection" endpoint markers...
    {
      glPointSize (GCODE_OPENGL_SMALL_POINT_SIZE);
      glColor3f (GCODE_OPENGL_SMALL_POINT_COLOR[0],
                 GCODE_OPENGL_SMALL_POINT_COLOR[1],
                 GCODE_OPENGL_SMALL_POINT_COLOR[2]);
      glBegin (GL_POINTS);
      glVertex3f (p0[0], p0[1], block->offset->z[0]);
      glVertex3f (p1[0], p1[1], block->offset->z[1]);
      glEnd ();
    }
    else                                                                        // ...or "discontinuity" endpoint markes, if there is a break;
    {
      other_block = block;                                                      // If we're here the block is NOT selected so data comes from the ordered list,

      gcode_get_circular_prev (&other_block);                                   // therefore the start point can be compared to the previous block's end point.

      other_block->ends (other_block, e0, e1, GCODE_GET_WITH_OFFSET);           // So we look up the previous block circularly, find its end point and compare;

      if (GCODE_MATH_2D_DISTANCE (e1, p0) > GCODE_TOLERANCE)                    // If they DON'T match, this is a break, so we draw a marker.
      {
        glPointSize (GCODE_OPENGL_BREAK_POINT_SIZE);
        glColor3f (GCODE_OPENGL_BREAK_POINT_COLOR[0],
                   GCODE_OPENGL_BREAK_POINT_COLOR[1],
                   GCODE_OPENGL_BREAK_POINT_COLOR[2]);
        glBegin (GL_POINTS);
        glVertex3f (p0[0], p0[1], block->offset->z[0]);
        glEnd ();
      }

      other_block = block;                                                      // Then we do the exact same thing for the next block's start point.

      gcode_get_circular_next (&other_block);

      other_block->ends (other_block, e0, e1, GCODE_GET_WITH_OFFSET);

      if (GCODE_MATH_2D_DISTANCE (p1, e0) > GCODE_TOLERANCE)                    // The thing to remember is that we're operating on a continuous sub-section
      {                                                                         // of an ordered snapshot of the original list, so even if this sub-chain is
        glPointSize (GCODE_OPENGL_BREAK_POINT_SIZE);                            // isolated from the rest of the sketch, it will close up without any breaks
        glColor3f (GCODE_OPENGL_BREAK_POINT_COLOR[0],                           // as long as it ends exactly where it started...
                   GCODE_OPENGL_BREAK_POINT_COLOR[1],
                   GCODE_OPENGL_BREAK_POINT_COLOR[2]);
        glBegin (GL_POINTS);
        glVertex3f (p1[0], p1[1], block->offset->z[1]);
        glEnd ();
      }
    }
  }
#endif
}

int
gcode_line_eval (gcode_block_t *block, gfloat_t y, gfloat_t *x_array, uint32_t *x_index)
{
  gcode_line_t *line;
  gcode_vec2d_t p0, p1, normal;
  gfloat_t dx, dy;

  line = (gcode_line_t *)block->pdata;

  gcode_line_with_offset (block, p0, p1, normal);

  dx = p0[0] - p1[0];
  dy = p0[1] - p1[1];

  /* y is outside the bounds of the line segment. */

  if (((y - GCODE_PRECISION > p0[1]) && (y - GCODE_PRECISION > p1[1])) ||
      ((y + GCODE_PRECISION < p0[1]) && (y + GCODE_PRECISION < p1[1])))
    return (1);

  if (fabs (dx) < GCODE_PRECISION)
  {
    /* Line is vertical, assign x-value from either of the points */

    x_array[(*x_index)++] = p0[0];
  }
  else if (fabs (dy) < GCODE_PRECISION)
  {
    /* Line is horizontal, assign both of the x-value points */

    x_array[(*x_index)++] = p0[0];
    x_array[(*x_index)++] = p1[0];
  }
  else
  {
    /**
     * y = ax + b
     * x = (y - b) / a
     * x = p0[0] + (y - p0[1]) / (dy / dx)
     */

    x_array[(*x_index)++] = p0[0] + (y - p0[1]) / (dy / dx);
  }

  return (0);
}

int
gcode_line_ends (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, uint8_t mode)
{
  gcode_line_t *line;
  gcode_vec2d_t normal;

  line = (gcode_line_t *)block->pdata;

  switch (mode)
  {
    case GCODE_GET:

      p0[0] = line->p0[0];
      p0[1] = line->p0[1];

      p1[0] = line->p1[0];
      p1[1] = line->p1[1];

      break;

    case GCODE_SET:

      line->p0[0] = p0[0];
      line->p0[1] = p0[1];

      line->p1[0] = p1[0];
      line->p1[1] = p1[1];

      break;

    case GCODE_GET_WITH_OFFSET:

      gcode_line_with_offset (block, p0, p1, normal);

      break;

    case GCODE_GET_NORMAL:

      if (GCODE_MATH_2D_MANHATTAN (line->p0, line->p1) < GCODE_PRECISION)
        return (1);

      gcode_line_with_offset (block, p0, p1, normal);

      p0[0] = normal[0];
      p0[1] = normal[1];
      p1[0] = normal[0];
      p1[1] = normal[1];

      break;

    case GCODE_GET_TANGENT:

      if (GCODE_MATH_2D_MANHATTAN (line->p0, line->p1) < GCODE_PRECISION)
        return (1);

      p0[0] = line->p1[0] - line->p0[0];
      p0[1] = line->p1[1] - line->p0[1];

      GCODE_MATH_VEC2D_UNITIZE (p0);

      p1[0] = p0[0];
      p1[1] = p0[1];

      break;

    default:

      return (1);
  }

  return (0);
}

int
gcode_line_midpoint (gcode_block_t *block, gcode_vec2d_t midpoint, uint8_t mode)
{
  gcode_line_t *line;
  gcode_vec2d_t p0, p1, normal;

  line = (gcode_line_t *)block->pdata;

  switch (mode)
  {
    case GCODE_GET:

      midpoint[0] = 0.5 * (line->p0[0] + line->p1[0]);
      midpoint[1] = 0.5 * (line->p0[1] + line->p1[1]);

      break;

    case GCODE_GET_WITH_OFFSET:

      gcode_line_with_offset (block, p0, p1, normal);

      midpoint[0] = 0.5 * (p0[0] + p1[0]);
      midpoint[1] = 0.5 * (p0[1] + p1[1]);

      break;

    default:

      return (1);
  }

  return (0);
}

void
gcode_line_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max)
{
  gcode_line_t *line;
  gcode_vec2d_t p0, p1, normal;

  line = (gcode_line_t *)block->pdata;

  gcode_line_with_offset (block, p0, p1, normal);

  min[0] = p0[0];
  min[1] = p0[1];
  max[0] = p0[0];
  max[1] = p0[1];

  if (p1[0] < min[0])
    min[0] = p1[0];

  if (p1[0] > max[0])
    max[0] = p1[0];

  if (p1[1] < min[1])
    min[1] = p1[1];

  if (p1[1] > max[1])
    max[1] = p1[1];
}

void
gcode_line_qdbb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max)
{
  gcode_line_t *line;

  line = (gcode_line_t *)block->pdata;

  min[0] = (line->p0[0] < line->p1[0]) ? line->p0[0] : line->p1[0];
  max[0] = (line->p0[0] > line->p1[0]) ? line->p0[0] : line->p1[0];
  min[1] = (line->p0[1] < line->p1[1]) ? line->p0[1] : line->p1[1];
  max[1] = (line->p0[1] > line->p1[1]) ? line->p0[1] : line->p1[1];

  min[0] -= GCODE_PRECISION;
  max[0] += GCODE_PRECISION;
  min[0] -= GCODE_PRECISION;
  max[0] += GCODE_PRECISION;
}

gfloat_t
gcode_line_length (gcode_block_t *block)
{
  gcode_line_t *line;
  gfloat_t length;

  line = (gcode_line_t *)block->pdata;

  GCODE_MATH_VEC2D_DIST (length, line->p0, line->p1);

  return (length);
}

void
gcode_line_move (gcode_block_t *block, gcode_vec2d_t delta)
{
  gcode_line_t *line;
  gcode_vec2d_t orgnl_pt, xform_pt;

  line = (gcode_line_t *)block->pdata;

  GCODE_MATH_VEC2D_COPY (orgnl_pt, line->p0);
  GCODE_MATH_TRANSLATE (xform_pt, orgnl_pt, delta);
  GCODE_MATH_VEC2D_COPY (line->p0, xform_pt);

  GCODE_MATH_VEC2D_COPY (orgnl_pt, line->p1);
  GCODE_MATH_TRANSLATE (xform_pt, orgnl_pt, delta);
  GCODE_MATH_VEC2D_COPY (line->p1, xform_pt);
}

void
gcode_line_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_line_t *line;
  gcode_vec2d_t orgnl_pt, xform_pt;

  line = (gcode_line_t *)block->pdata;

  GCODE_MATH_VEC2D_SUB (orgnl_pt, line->p0, datum);
  GCODE_MATH_ROTATE (xform_pt, orgnl_pt, angle);
  GCODE_MATH_VEC2D_ADD (line->p0, xform_pt, datum);

  GCODE_MATH_VEC2D_SUB (orgnl_pt, line->p1, datum);
  GCODE_MATH_ROTATE (xform_pt, orgnl_pt, angle);
  GCODE_MATH_VEC2D_ADD (line->p1, xform_pt, datum);
}

void
gcode_line_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)     // Flips the line around an axis through a point, not the endpoints of the line
{
  gcode_line_t *line;

  line = (gcode_line_t *)block->pdata;

  if (GCODE_MATH_IS_EQUAL (angle, 0))
  {
    line->p0[1] -= datum[1];
    line->p0[1] = -line->p0[1];
    line->p0[1] += datum[1];

    line->p1[1] -= datum[1];
    line->p1[1] = -line->p1[1];
    line->p1[1] += datum[1];
  }

  if (GCODE_MATH_IS_EQUAL (angle, 90))
  {
    line->p0[0] -= datum[0];
    line->p0[0] = -line->p0[0];
    line->p0[0] += datum[0];

    line->p1[0] -= datum[0];
    line->p1[0] = -line->p1[0];
    line->p1[0] += datum[0];
  }
}

void
gcode_line_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_line_t *line;

  line = (gcode_line_t *)block->pdata;

  line->p0[0] *= scale;
  line->p0[1] *= scale;
  line->p1[0] *= scale;
  line->p1[1] *= scale;
}

void
gcode_line_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_line_t *line;

  line = (gcode_line_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_LINE_START_POINT) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_FLT (xyz, value))
        for (int j = 0; j < 2; j++)
          line->p0[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_LINE_END_POINT) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_FLT (xyz, value))
        for (int j = 0; j < 2; j++)
          line->p1[j] = (gfloat_t)xyz[j];
    }
  }
}

void
gcode_line_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_line_t *line, *model_line;

  model_line = (gcode_line_t *)model->pdata;

  gcode_line_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  line = (gcode_line_t *)(*block)->pdata;

  line->p0[0] = model_line->p0[0];
  line->p0[1] = model_line->p0[1];
  line->p1[0] = model_line->p1[0];
  line->p1[1] = model_line->p1[1];
}

/**
 * Based on the line data retrieved from 'block' and the offset data referenced
 * by 'block's offset pointer, calculate the endpoints of- and the normal vector
 * to a new line that is first rotated and translated by 'offset->rotation' and 
 * 'offset->origin' then also shifted "sideways" (in a direction perpendicular 
 * to the line) by 'offset->tool' plus 'offset->eval' on the side determined by 
 * 'offset->side', such as to form a line that would be parallel with the result
 * of the original roto-translation.
 */

void
gcode_line_with_offset (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, gcode_vec2d_t normal)
{
  gcode_line_t *line;
  gcode_vec2d_t xform_p0, xform_p1, normal_offset;
  gfloat_t inv_mag, line_dx, line_dy;

  line = (gcode_line_t *)block->pdata;

  /* Transform endpoints with rotation and translation from block offset */
  GCODE_MATH_ROTATE (xform_p0, line->p0, block->offset->rotation);
  GCODE_MATH_ROTATE (xform_p1, line->p1, block->offset->rotation);
  GCODE_MATH_TRANSLATE (xform_p0, xform_p0, block->offset->origin);
  GCODE_MATH_TRANSLATE (xform_p1, xform_p1, block->offset->origin);

  /* Calculate the slope to the line */
  line_dx = xform_p0[0] - xform_p1[0];
  line_dy = xform_p0[1] - xform_p1[1];

  /* Based on the slope, calculate the normal vector to the line */
  normal[0] = -line_dy;
  normal[1] = line_dx;

  /* Calculate the factor that will 'unitize' the normal vector */
  inv_mag = 1.0 / GCODE_MATH_2D_MAGNITUDE (normal);

  /* Rescale normal vector to unity and apply side selection factor */
  normal[0] *= inv_mag * block->offset->side;
  normal[1] *= inv_mag * block->offset->side;

  /* Calculate 'sideways' shift resulting from 'tool' and 'eval' factors */
  normal_offset[0] = normal[0] * (block->offset->eval + block->offset->tool);
  normal_offset[1] = normal[1] * (block->offset->eval + block->offset->tool);

  /* Calculate new endpoints with 'sideways' shift applied */
  p0[0] = xform_p0[0] + normal_offset[0];
  p0[1] = xform_p0[1] + normal_offset[1];

  p1[0] = xform_p1[0] + normal_offset[0];
  p1[1] = xform_p1[1] + normal_offset[1];
}

void
gcode_line_flip_direction (gcode_block_t *block)
{
  gcode_line_t *line;
  gcode_vec2d_t swap;

  line = (gcode_line_t *)block->pdata;

  swap[0] = line->p0[0];
  swap[1] = line->p0[1];

  line->p0[0] = line->p1[0];
  line->p0[1] = line->p1[1];

  line->p1[0] = swap[0];
  line->p1[1] = swap[1];
}
