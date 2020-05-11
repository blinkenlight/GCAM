/**
 *  gcode_arc.c
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

#include "gui_define.h"
#include "gcode_arc.h"
#include "gcode.h"

// Number of segments to use to approximate curves other than a circular arc;
#define ARCSEGMENTS 50

void
gcode_arc_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_arc_t *arc;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_ARC, 0);

  (*block)->free = gcode_arc_free;
  (*block)->save = gcode_arc_save;
  (*block)->load = gcode_arc_load;
  (*block)->make = gcode_arc_make;
  (*block)->draw = gcode_arc_draw;
  (*block)->eval = gcode_arc_eval;
  (*block)->ends = gcode_arc_ends;
  (*block)->aabb = gcode_arc_aabb;
  (*block)->length = gcode_arc_length;
  (*block)->move = gcode_arc_move;
  (*block)->spin = gcode_arc_spin;
  (*block)->flip = gcode_arc_flip;
  (*block)->scale = gcode_arc_scale;
  (*block)->parse = gcode_arc_parse;
  (*block)->clone = gcode_arc_clone;

  (*block)->pdata = malloc (sizeof (gcode_arc_t));

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Arc");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  arc = (gcode_arc_t *)(*block)->pdata;

  arc->p[0] = 0.0;
  arc->p[1] = 0.0;
  arc->radius = GCODE_UNITS ((*block)->gcode, 0.5);
  arc->start_angle = 180.0;
  arc->sweep_angle = -90.0;
  arc->native_mode = GCODE_ARC_INTERFACE_SWEEP;
}

void
gcode_arc_free (gcode_block_t **block)
{
  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_arc_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_arc_t *arc;
  uint32_t size;
  uint8_t data;

  arc = (gcode_arc_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_ARC);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_2D_FLT (fh, GCODE_XML_ATTR_ARC_START_POINT, arc->p);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_ARC_RADIUS, arc->radius);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_ARC_START_ANGLE, arc->start_angle);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_ARC_SWEEP_ANGLE, arc->sweep_angle);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_ARC_INTERFACE, arc->native_mode);
    GCODE_WRITE_XML_CL_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    GCODE_WRITE_BINARY_1X_POINT (fh, GCODE_BIN_DATA_ARC_START_POINT, arc->p);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_ARC_RADIUS, sizeof (gfloat_t), &arc->radius);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_ARC_START_ANGLE, sizeof (gfloat_t), &arc->start_angle);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_ARC_SWEEP_ANGLE, sizeof (gfloat_t), &arc->sweep_angle);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_ARC_INTERFACE, sizeof (uint8_t), &arc->native_mode);
  }
}

void
gcode_arc_load (gcode_block_t *block, FILE *fh)
{
  gcode_arc_t *arc;
  uint32_t bsize, dsize, start;
  uint8_t data;

  arc = (gcode_arc_t *)block->pdata;

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

      case GCODE_BIN_DATA_ARC_START_POINT:
        fread (arc->p, sizeof (gfloat_t), 2, fh);
        break;

      case GCODE_BIN_DATA_ARC_RADIUS:
        fread (&arc->radius, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_ARC_START_ANGLE:
        fread (&arc->start_angle, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_ARC_SWEEP_ANGLE:
        fread (&arc->sweep_angle, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_ARC_INTERFACE:
        fread (&arc->native_mode, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_arc_make (gcode_block_t *block)
{
  gcode_arc_t *arc;
  gcode_vec2d_t p0, origin, center;
  gfloat_t arc_radius_offset, start_angle;
  char string[256];

  arc = (gcode_arc_t *)block->pdata;

  GCODE_CLEAR (block);

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  sprintf (string, "ARC: %s", block->comment);

  gcode_arc_with_offset (block, origin, center, p0, &arc_radius_offset, &start_angle);

  /* Do not proceed if the radius is effectively zero (or even negative!) */

  if (arc_radius_offset < GCODE_PRECISION)
    return;

  /* Do not proceed if the endpoints are the same but the sweep is not 360 */

  if (GCODE_MATH_IS_EQUAL (origin[0], p0[0]) &&
      GCODE_MATH_IS_EQUAL (origin[1], p0[1]) &&
      !GCODE_MATH_IS_EQUAL (fabs (arc->sweep_angle), 360.0))
    return;

  GCODE_2D_LINE (block, origin[0], origin[1], "");

  if (arc->sweep_angle < 0.0)
  {
    /* Clockwise */
    if (fabs (block->offset->z[0] - block->offset->z[1]) < GCODE_PRECISION)
    {
      GCODE_2D_ARC_CW (block, p0[0], p0[1], center[0] - origin[0], center[1] - origin[1], string);
    }
    else
    {
      GCODE_3D_ARC_CW (block, p0[0], p0[1], block->offset->z[1], center[0] - origin[0], center[1] - origin[1], string);
    }
  }
  else
  {
    /* Counter-Clockwise */
    if (fabs (block->offset->z[0] - block->offset->z[1]) < GCODE_PRECISION)
    {
      GCODE_2D_ARC_CCW (block, p0[0], p0[1], center[0] - origin[0], center[1] - origin[1], string);
    }
    else
    {
      GCODE_3D_ARC_CCW (block, p0[0], p0[1], block->offset->z[1], center[0] - origin[0], center[1] - origin[1], string);
    }
  }
}

void
gcode_arc_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_block_t *other_block;
  gcode_arc_t *arc;
  gcode_vec2d_t e0, e1, p0, p1, cp;
  gfloat_t radius, start_angle, coef, t;
  uint32_t n, sindex, edited, picked;

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
    arc = (gcode_arc_t *)selected->pdata;                                       // no longer reflects original segment direction, we need to take our data
    gcode_arc_with_offset (selected, p0, cp, p1, &radius, &start_angle);        // from somewhere else if direction is important. Luckily, the only such case
  }                                                                             // (drawing the currently selected block with a start-to-end gradient stroke)
  else                                                                          // can be handled fairly simply considering we also happen to have a reference
  {                                                                             // to the selected block in its original, untampered form: we just use that
    arc = (gcode_arc_t *)block->pdata;                                          // as the data source instead of the provided "snapshot" if the current block
    gcode_arc_with_offset (block, p0, cp, p1, &radius, &start_angle);           // happens to be the selected one...
  }

  if ((radius < GCODE_PRECISION) && !picked)                                    // Do not display this arc if it's got a 0 radius and it's not selected;
    return;

  glLoadName ((GLuint) block->name);                                            // Attach the block's "name" to the arc being drawn for reverse lookup;
  glLineWidth (1);

  glBegin (GL_LINE_STRIP);

  for (n = 0; n <= ARCSEGMENTS; n++)                                            // Arcs get actually drawn as a sequence of line segments;
  {
    t = (gfloat_t)n / ARCSEGMENTS;                                              // Can't loop directly on 't' - it would have issues comparing to "1";

    coef = picked ? 0.5 + t / 2.0 : 1.0;                                        // The current point color is a gradient from 50% to 100% if the arc is selected;

    glColor3f (coef * GCODE_OPENGL_SELECTABLE_COLORS[sindex][0],
               coef * GCODE_OPENGL_SELECTABLE_COLORS[sindex][1],
               coef * GCODE_OPENGL_SELECTABLE_COLORS[sindex][2]);
    glVertex3f (cp[0] + radius * cos ((start_angle + t * arc->sweep_angle) * GCODE_DEG2RAD),
                cp[1] + radius * sin ((start_angle + t * arc->sweep_angle) * GCODE_DEG2RAD),
                block->offset->z[0] * (1.0 - t) + block->offset->z[1] * t);
  }

  glEnd ();                                                                     // The arc itself is drawn now but we might still need to draw end markers;

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
gcode_arc_eval (gcode_block_t *block, gfloat_t y, gfloat_t *x_array, uint32_t *x_index)
{
  gcode_arc_t *arc;
  gfloat_t angle1, angle2, start_angle, end_angle, arc_radius, arc_start_angle;
  gcode_vec2d_t arc_p0, arc_center, arc_p1;
  int fail;

  arc = (gcode_arc_t *)block->pdata;

  /* Transform */
  gcode_arc_with_offset (block, arc_p0, arc_center, arc_p1, &arc_radius, &arc_start_angle);

  if (arc_radius < GCODE_PRECISION)
    return (1);

  /**
   * Work-around for assuring things that should intersect, do. Without this,
   * calculated objects could JUST miss a raster line with nasty consequences.
   */

  arc_radius += GCODE_PRECISION_FLOOR;

  /* Check whether y is outside of the circle boundaries */
  if (arc_radius < GCODE_MATH_1D_DISTANCE (arc_center[1], y))
    return (1);

  /* y is now in unit circle coordinates */
  y = (y - arc_center[1]) / arc_radius;

  /* Take the arcsin to get the angles */
  angle1 = GCODE_RAD2DEG * asin (y);
  angle2 = 180.0 - angle1;

  GCODE_MATH_WRAP_TO_360_DEGREES (angle1);

  /**
   * Notes:
   * angle2 can never be negative as angle1 is always within +/- 90 degrees;
   * angle1 represents quadrants 1 and 4. (+X)
   * angle2 represents quadrants 2 and 3. (-X)
   */

  fail = 1;

  /**
   * If the two intersection angles are effectively indistinguishable, they are
   * probably a single point of tangency and returning both would be a mistake.
   * Also, if the arc does not have an endpoint coincident with that tangency
   * point, returning anything at all would be a mistake as well: the pocketing
   * algorithm considers each intersection a passage between the outside and the
   * inside of the contour, which a point of tangency clearly wouldn't be if the
   * arc in question curves away from it in both directions. Therefore, if this
   * is indeed a tangency point, we return either a single point or none at all.
   */

  if (GCODE_MATH_DIFFERENCE (angle1, angle2) < GCODE_ANGULAR_PRECISION)
  {
    gfloat_t angle;

    angle = (angle1 + angle2) / 2;

    if ((GCODE_MATH_1D_DISTANCE (arc_p0[0], arc_center[0]) > GCODE_PRECISION) &&
        (GCODE_MATH_1D_DISTANCE (arc_p1[0], arc_center[0]) > GCODE_PRECISION))
      return(fail);

    if (gcode_math_angle_within_arc (arc_start_angle, arc->sweep_angle, angle) == 0)
    {
      x_array[*x_index] = arc_center[0] + arc_radius * cos (GCODE_DEG2RAD * angle);

      (*x_index)++;

      fail = 0;
    }
  }
  else
  {
    if (gcode_math_angle_within_arc (arc_start_angle, arc->sweep_angle, angle1) == 0)
    {
      x_array[*x_index] = arc_center[0] + arc_radius * cos (GCODE_DEG2RAD * angle1);

      (*x_index)++;

      fail = 0;
    }

    if (gcode_math_angle_within_arc (arc_start_angle, arc->sweep_angle, angle2) == 0)
    {
      x_array[*x_index] = arc_center[0] + arc_radius * cos (GCODE_DEG2RAD * angle2);

      (*x_index)++;

      fail = 0;
    }
  }

  return (fail);
}

int
gcode_arc_ends (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, uint8_t mode)
{
  gcode_arc_t *arc;

  arc = (gcode_arc_t *)block->pdata;

  switch (mode)
  {
    case GCODE_GET:
    {
      gcode_vec2d_t center;

      p0[0] = arc->p[0];
      p0[1] = arc->p[1];

      center[0] = p0[0] - arc->radius * cos (arc->start_angle * GCODE_DEG2RAD);
      center[1] = p0[1] - arc->radius * sin (arc->start_angle * GCODE_DEG2RAD);

      p1[0] = center[0] + arc->radius * cos ((arc->start_angle + arc->sweep_angle) * GCODE_DEG2RAD);
      p1[1] = center[1] + arc->radius * sin ((arc->start_angle + arc->sweep_angle) * GCODE_DEG2RAD);

      break;
    }

    case GCODE_SET:
    {
      gcode_arcdata_t arcdata;

      arcdata.p0[0] = p0[0];
      arcdata.p0[1] = p0[1];

      arcdata.p1[0] = p1[0];
      arcdata.p1[1] = p1[1];

      arcdata.radius = arc->radius;

      arcdata.fls = (arc->sweep_angle > 0.0) ? 1 : 0;
      arcdata.fla = (fabs (arc->sweep_angle) > 180.0) ? 1 : 0;

      if (gcode_arc_radius_to_sweep (&arcdata) != 0)
        return (1);

      arc->p[0] = p0[0];
      arc->p[1] = p0[1];

      arc->start_angle = arcdata.start_angle;
      arc->sweep_angle = arcdata.sweep_angle;

      break;
    }

    case GCODE_GET_WITH_OFFSET:
    {
      gcode_vec2d_t origin, center;
      gfloat_t arc_radius_offset, start_angle;

      gcode_arc_with_offset (block, origin, center, p0, &arc_radius_offset, &start_angle);

      p1[0] = p0[0];
      p1[1] = p0[1];

      p0[0] = origin[0];
      p0[1] = origin[1];

      break;
    }

    case GCODE_GET_NORMAL:
    {
      gfloat_t xform_angle, flip;

      xform_angle = arc->start_angle + block->offset->rotation;
      flip = block->offset->side * (arc->sweep_angle < 0.0 ? -1.0 : 1.0);

      p0[0] = flip * cos (xform_angle * GCODE_DEG2RAD);
      p0[1] = flip * sin (xform_angle * GCODE_DEG2RAD);

      p1[0] = flip * cos ((xform_angle + arc->sweep_angle) * GCODE_DEG2RAD);
      p1[1] = flip * sin ((xform_angle + arc->sweep_angle) * GCODE_DEG2RAD);

      break;
    }

    case GCODE_GET_TANGENT:
    {
      gfloat_t enter_angle, leave_angle;

      enter_angle = arc->sweep_angle < 0 ? arc->start_angle - 90 : arc->start_angle + 90;

      GCODE_MATH_WRAP_TO_360_DEGREES (enter_angle);

      p0[0] = cos (GCODE_DEG2RAD * enter_angle);
      p0[1] = sin (GCODE_DEG2RAD * enter_angle);

      leave_angle = enter_angle + arc->sweep_angle;

      GCODE_MATH_WRAP_TO_360_DEGREES (leave_angle);

      p1[0] = cos (GCODE_DEG2RAD * leave_angle);
      p1[1] = sin (GCODE_DEG2RAD * leave_angle);

      break;
    }

    case GCODE_GET_ALPHA:
    {
      gcode_vec2d_t center;

      p0[0] = p1[0] = arc->p[0];
      p0[1] = p1[1] = arc->p[1];

      break;
    }

    case GCODE_GET_OMEGA:
    {
      gcode_vec2d_t center;

      center[0] = arc->p[0] - arc->radius * cos (arc->start_angle * GCODE_DEG2RAD);
      center[1] = arc->p[1] - arc->radius * sin (arc->start_angle * GCODE_DEG2RAD);

      p0[0] = p1[0] = center[0] + arc->radius * cos ((arc->start_angle + arc->sweep_angle) * GCODE_DEG2RAD);
      p0[1] = p1[1] = center[1] + arc->radius * sin ((arc->start_angle + arc->sweep_angle) * GCODE_DEG2RAD);

      break;
    }

    default:

      return (1);
  }

  return (0);
}

int
gcode_arc_center (gcode_block_t *block, gcode_vec2d_t center, uint8_t mode)
{
  gcode_arc_t *arc;

  arc = (gcode_arc_t *)block->pdata;

  switch (mode)
  {
    case GCODE_GET:

      center[0] = arc->p[0] - arc->radius * cos (arc->start_angle * GCODE_DEG2RAD);
      center[1] = arc->p[1] - arc->radius * sin (arc->start_angle * GCODE_DEG2RAD);

      break;

    case GCODE_GET_WITH_OFFSET:

      center[0] = arc->p[0] - arc->radius * cos (arc->start_angle * GCODE_DEG2RAD);
      center[1] = arc->p[1] - arc->radius * sin (arc->start_angle * GCODE_DEG2RAD);

      GCODE_MATH_ROTATE (center, center, block->offset->rotation);              // NOTE: "rotating" the center is not as absurd as it sounds:
      GCODE_MATH_TRANSLATE (center, center, block->offset->origin);             // remember that we're NOT rotating the center AROUND ITSELF.

      break;

    default:

      return (1);
  }

  return (0);
}

int
gcode_arc_midpoint (gcode_block_t *block, gcode_vec2d_t midpoint, uint8_t mode)
{
  gcode_arc_t *arc;
  gcode_vec2d_t p0, p1, center;
  gfloat_t radius, start_angle;

  arc = (gcode_arc_t *)block->pdata;

  switch (mode)
  {
    case GCODE_GET:

      gcode_arc_center (block, center, GCODE_GET);

      midpoint[0] = center[0] + arc->radius * cos (GCODE_DEG2RAD * (arc->start_angle + arc->sweep_angle * 0.5));
      midpoint[1] = center[1] + arc->radius * sin (GCODE_DEG2RAD * (arc->start_angle + arc->sweep_angle * 0.5));

      break;

    case GCODE_GET_WITH_OFFSET:

      gcode_arc_with_offset (block, p0, center, p1, &radius, &start_angle);

      midpoint[0] = center[0] + radius * cos (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));
      midpoint[1] = center[1] + radius * sin (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));

      break;

    default:

      return (1);
  }

  return (0);
}

void
gcode_arc_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max)
{
  gcode_arc_t *arc;
  gcode_vec2d_t origin, center, end_pos;
  gfloat_t arc_radius_offset, start_angle;

  arc = (gcode_arc_t *)block->pdata;

  gcode_arc_with_offset (block, origin, center, end_pos, &arc_radius_offset, &start_angle);

  /* Use start and end points to check for min and max */
  min[0] = origin[0];
  min[1] = origin[1];
  max[0] = origin[0];
  max[1] = origin[1];

  if (end_pos[0] < min[0])
    min[0] = end_pos[0];

  if (end_pos[0] > max[0])
    max[0] = end_pos[0];

  if (end_pos[1] < min[1])
    min[1] = end_pos[1];

  if (end_pos[1] > max[1])
    max[1] = end_pos[1];

  /* Test if arc intersects X or Y axis with respect to arc center */
  if (gcode_math_angle_within_arc (start_angle, arc->sweep_angle, 0.0) == 0)
    max[0] = center[0] + arc_radius_offset;

  if (gcode_math_angle_within_arc (start_angle, arc->sweep_angle, 90.0) == 0)
    max[1] = center[1] + arc_radius_offset;

  if (gcode_math_angle_within_arc (start_angle, arc->sweep_angle, 180.0) == 0)
    min[0] = center[0] - arc_radius_offset;

  if (gcode_math_angle_within_arc (start_angle, arc->sweep_angle, 270.0) == 0)
    min[1] = center[1] - arc_radius_offset;
}

void
gcode_arc_qdbb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max)
{
  gcode_arc_t *arc;
  gcode_vec2d_t center;

  arc = (gcode_arc_t *)block->pdata;

  gcode_arc_center (block, center, GCODE_GET);

  min[0] = center[0] - arc->radius - GCODE_PRECISION;
  max[0] = center[0] + arc->radius + GCODE_PRECISION;
  min[1] = center[1] - arc->radius - GCODE_PRECISION;
  max[1] = center[1] + arc->radius + GCODE_PRECISION;
}

gfloat_t
gcode_arc_length (gcode_block_t *block)
{
  gcode_arc_t *arc;
  gfloat_t length;

  arc = (gcode_arc_t *)block->pdata;

  length = fabs (arc->radius * GCODE_2PI * arc->sweep_angle / 360.0);

  return (length);
}

void
gcode_arc_move (gcode_block_t *block, gcode_vec2d_t delta)
{
  gcode_arc_t *arc;
  gcode_vec2d_t orgnl_pt, xform_pt;

  arc = (gcode_arc_t *)block->pdata;

  GCODE_MATH_VEC2D_COPY (orgnl_pt, arc->p);
  GCODE_MATH_TRANSLATE (xform_pt, orgnl_pt, delta);
  GCODE_MATH_VEC2D_COPY (arc->p, xform_pt);
}

void
gcode_arc_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_arc_t *arc;
  gcode_vec2d_t orgnl_pt, xform_pt;

  arc = (gcode_arc_t *)block->pdata;

  GCODE_MATH_VEC2D_SUB (orgnl_pt, arc->p, datum);
  GCODE_MATH_ROTATE (xform_pt, orgnl_pt, angle);
  GCODE_MATH_VEC2D_ADD (arc->p, xform_pt, datum);

  arc->start_angle += angle;
  GCODE_MATH_WRAP_TO_360_DEGREES (arc->start_angle);
}

void
gcode_arc_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)      // Flips the arc around an axis through a point, not the endpoints of the arc
{
  gcode_arc_t *arc;

  arc = (gcode_arc_t *)block->pdata;

  if (GCODE_MATH_IS_EQUAL (angle, 0))
  {
    arc->p[1] -= datum[1];
    arc->p[1] = -arc->p[1];
    arc->p[1] += datum[1];

    arc->start_angle = 360 - arc->start_angle;
    arc->sweep_angle = -arc->sweep_angle;
    GCODE_MATH_WRAP_TO_360_DEGREES(arc->start_angle);
  }

  if (GCODE_MATH_IS_EQUAL (angle, 90))
  {
    arc->p[0] -= datum[0];
    arc->p[0] = -arc->p[0];
    arc->p[0] += datum[0];

    arc->start_angle = 180 - arc->start_angle;
    arc->sweep_angle = -arc->sweep_angle;
    GCODE_MATH_WRAP_TO_360_DEGREES(arc->start_angle);
  }
}

void
gcode_arc_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_arc_t *arc;

  arc = (gcode_arc_t *)block->pdata;

  arc->p[0] *= scale;
  arc->p[1] *= scale;
  arc->radius *= scale;
}

void
gcode_arc_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_arc_t *arc;

  arc = (gcode_arc_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_ARC_START_POINT) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_FLT (xyz, value))
        for (int j = 0; j < 2; j++)
          arc->p[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_ARC_RADIUS) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        arc->radius = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_ARC_START_ANGLE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        arc->start_angle = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_ARC_SWEEP_ANGLE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        arc->sweep_angle = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_ARC_INTERFACE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        arc->native_mode = m;
    }

    GCODE_MATH_WRAP_TO_360_DEGREES (arc->start_angle);                          // No, you can't start at 5000 degrees...
    GCODE_MATH_SNAP_TO_720_DEGREES (arc->sweep_angle);                          // ...nor can you sweep 1000000 degrees;
  }
}

void
gcode_arc_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_arc_t *arc, *model_arc;

  model_arc = (gcode_arc_t *)model->pdata;

  gcode_arc_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  arc = (gcode_arc_t *)(*block)->pdata;

  arc->p[0] = model_arc->p[0];
  arc->p[1] = model_arc->p[1];

  arc->radius = model_arc->radius;
  arc->start_angle = model_arc->start_angle;
  arc->sweep_angle = model_arc->sweep_angle;
  arc->native_mode = model_arc->native_mode;
}

/**
 * Based on the arc data retrieved from 'block' and the offset data referenced
 * by 'block's offset pointer, calculate the parameters of an arc that is first
 * rotated and translated by 'offset->rotation' and 'offset->origin' then also
 * shifted radially "in" or "out" by 'offset->tool' plus 'offset->eval' in the
 * direction determined by 'offset->side', such as to form an arc that would be
 * "parallel" with the result of the original roto-translation.
 */

void
gcode_arc_with_offset (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t center, gcode_vec2d_t p1, gfloat_t *radius, gfloat_t *start_angle)
{
  gcode_arc_t *arc;
  gcode_vec2d_t xform_p0, xform_p1;
  gcode_vec2d_t native_cp, xform_cp;
  gfloat_t xform_radius, xform_start_angle;
  gfloat_t flip;

  arc = (gcode_arc_t *)block->pdata;

  /* Obtain coordinates of center from start point and start angle */
  native_cp[0] = arc->p[0] - arc->radius * cos (arc->start_angle * GCODE_DEG2RAD);
  native_cp[1] = arc->p[1] - arc->radius * sin (arc->start_angle * GCODE_DEG2RAD);

  /* Transform center with rotation and translation from block offset */
  GCODE_MATH_ROTATE (xform_cp, native_cp, block->offset->rotation);             // NOTE: "rotating" the center is not as absurd as it sounds:
  GCODE_MATH_TRANSLATE (xform_cp, xform_cp, block->offset->origin);             // remember that we're NOT rotating the center AROUND ITSELF.

  /* Transform start angle with rotation from block offset */
  xform_start_angle = arc->start_angle + block->offset->rotation;

  /* Prevent rounding fuzz and fmod() fuck-ups */
  GCODE_MATH_WRAP_TO_360_DEGREES (xform_start_angle);
  GCODE_MATH_SNAP_TO_360_DEGREES (xform_start_angle);

  /* Create side selection factor depending on offset direction/side */
  flip = block->offset->side * (arc->sweep_angle < 0.0 ? -1.0 : 1.0);

  /* Offset original radius in or out by 'tool' and 'eval' factors */
  xform_radius = arc->radius + flip * (block->offset->tool + block->offset->eval);

  /* Prevent negative radii */
  if (xform_radius < 0.0)
    xform_radius = 0.0;

  /* Calculate new start and end points from new center, radius and angles */
  xform_p0[0] = xform_cp[0] + xform_radius * cos (xform_start_angle * GCODE_DEG2RAD);
  xform_p0[1] = xform_cp[1] + xform_radius * sin (xform_start_angle * GCODE_DEG2RAD);

  xform_p1[0] = xform_cp[0] + xform_radius * cos ((xform_start_angle + arc->sweep_angle) * GCODE_DEG2RAD);
  xform_p1[1] = xform_cp[1] + xform_radius * sin ((xform_start_angle + arc->sweep_angle) * GCODE_DEG2RAD);

  p0[0] = xform_p0[0];
  p0[1] = xform_p0[1];

  p1[0] = xform_p1[0];
  p1[1] = xform_p1[1];

  center[0] = xform_cp[0];
  center[1] = xform_cp[1];

  *radius = xform_radius;
  *start_angle = xform_start_angle;
}

/**
 * Invert the endpoints of 'block' - because arcs only explicitly define their
 * starting point (not the ending one), this requires finding the former ending
 * point and the new starting angle for the reversed arc (sweep simply inverts).
 */

void
gcode_arc_flip_direction (gcode_block_t *block)
{
  gcode_arc_t *arc;
  gcode_vec2d_t center;

  arc = (gcode_arc_t *)block->pdata;

  center[0] = arc->p[0] - arc->radius * cos (arc->start_angle * GCODE_DEG2RAD);
  center[1] = arc->p[1] - arc->radius * sin (arc->start_angle * GCODE_DEG2RAD);

  arc->p[0] = center[0] + arc->radius * cos ((arc->start_angle + arc->sweep_angle) * GCODE_DEG2RAD);
  arc->p[1] = center[1] + arc->radius * sin ((arc->start_angle + arc->sweep_angle) * GCODE_DEG2RAD);

  arc->start_angle = arc->start_angle + arc->sweep_angle;

  GCODE_MATH_WRAP_TO_360_DEGREES (arc->start_angle);                            // Yes, fmod (x, 360.0) DOES sometimes return "360.0". Yes, that would be BAD.
  GCODE_MATH_SNAP_TO_360_DEGREES (arc->start_angle);

  arc->sweep_angle *= -1.0;
}

/**
 * Not really a full-blown universal math routine, this explicitly services the
 * radius-to-sweep conversion needed for the GUI "arc" tab "radius" interface;
 * It takes two endpoints, the radius, and the "large arc" and "sweep direction"
 * flags and calculates the corresponding starting angle and sweep angle;
 * NOTE: there are edge cases where a unique solution is not defined; for these,
 * "1" is returned instead of "0" implying the input data cannot be applied.
 * NOTE: Implements the algorithm from 'SVG Implementation Notes' at W3.org;
 */

int
gcode_arc_radius_to_sweep (gcode_arcdata_t *arc)
{
  gfloat_t x1, y1, x2, y2, cx, cy;
  gfloat_t xp, yp, cxp, cyp;
  gfloat_t ux, uy, vx, vy;
  gfloat_t d, r, factor;
  gfloat_t theta, sweep;
  uint8_t fla, fls;

  d = GCODE_MATH_2D_DISTANCE (arc->p0, arc->p1);

  if (d < GCODE_PRECISION)                                                      // Full circle arcs have no calculable starting angle;
    return (1);

  if (arc->radius < GCODE_PRECISION)                                            // Zero radius arcs are not valid in GCAM;
    return (1);

  if (d > arc->radius * 2.0)                                                    // Arcs spanning more than their diameter are not possible;
    return (1);

  x1 = arc->p0[0];
  y1 = arc->p0[1];
  x2 = arc->p1[0];
  y2 = arc->p1[1];
  r = arc->radius;

  fla = arc->fla;
  fls = arc->fls;

  xp = (x1 - x2) / 2;
  yp = (y1 - y2) / 2;

  factor = sqrt ((r * r - yp * yp - xp * xp) / (yp * yp + xp * xp));

  factor = (fla == fls) ? 0 - factor : factor;

  cxp = 0 + factor * yp;
  cyp = 0 - factor * xp;

  cx = cxp + (x1 + x2) / 2;
  cy = cyp + (y1 + y2) / 2;

  ux = 1;
  uy = 0;

  vx = (xp - cxp) / r;
  vy = (yp - cyp) / r;

  theta = acos ((ux * vx + uy * vy) / (sqrt (ux * ux + uy * uy) * sqrt (vx * vx + vy * vy)));

  theta = ((ux * vy - uy * vx) < 0) ? 0 - theta : theta;

  theta = GCODE_RAD2DEG * theta;

  GCODE_MATH_WRAP_TO_360_DEGREES (theta);                                       // Yes, fmod (x, 360.0) DOES sometimes return "360.0". Yes, that would be BAD.
  GCODE_MATH_SNAP_TO_360_DEGREES (theta);

  ux = vx;
  uy = vy;

  vx = (0 - xp - cxp) / r;
  vy = (0 - yp - cyp) / r;

  sweep = acos ((ux * vx + uy * vy) / (sqrt (ux * ux + uy * uy) * sqrt (vx * vx + vy * vy)));

  sweep = ((ux * vy - uy * vx) < 0) ? 0 - sweep : sweep;

  sweep = GCODE_RAD2DEG * sweep;

  if (fls == 0)
  {
    if (sweep > 0)
      sweep -= 360.0;
  }
  else
  {
    if (sweep < 0)
      sweep += 360.0;
  }

  arc->cp[0] = cx;
  arc->cp[1] = cy;

  arc->start_angle = theta;
  arc->sweep_angle = sweep;

  return (0);
}

/**
 * Not really a full-blown universal math routine, this explicitly services the
 * center-to-sweep conversion needed for the GUI "arc" tab "center" interface;
 * It takes two endpoints, the center, and "sweep direction" flag and calculates
 * the corresponding starting angle and sweep angle (but not the "large" flag);
 * NOTE: there are edge cases where a unique solution is not defined; for these,
 * "1" is returned instead of "0" implying the input data cannot be applied.
 * NOTE: Implements the algorithm from 'SVG Implementation Notes' at W3.org;
 */

int
gcode_arc_center_to_sweep (gcode_arcdata_t *arc)
{
  gfloat_t x1, y1, x2, y2, cx, cy;
  gfloat_t ux, uy, vx, vy;
  gfloat_t r, factor;
  gfloat_t d, d1, d2;
  gfloat_t theta, sweep, sweep1, sweep2;
  uint8_t fls;

  d = GCODE_MATH_2D_DISTANCE (arc->p0, arc->p1);
  d1 = GCODE_MATH_2D_DISTANCE (arc->p0, arc->cp);
  d2 = GCODE_MATH_2D_DISTANCE (arc->p1, arc->cp);

  if (d < GCODE_PRECISION)                                                      // Full circle arcs have no calculable starting angle;
    return (1);

  if ((d1 + d1) / 2 < GCODE_PRECISION)                                          // Zero radius arcs are not valid in GCAM;
    return (1);

  if (fabs (d1 - d2) > GCODE_PRECISION)                                         // Elliptic arcs don't exist IN GCAM;
    return (1);

  x1 = arc->p0[0];
  y1 = arc->p0[1];
  x2 = arc->p1[0];
  y2 = arc->p1[1];
  cx = arc->cp[0];
  cy = arc->cp[1];

  fls = arc->fls;

  r = (d1 + d2) / 2;

  ux = 1;
  uy = 0;

  vx = (x1 - cx) / r;
  vy = (y1 - cy) / r;

  theta = acos ((ux * vx + uy * vy) / (sqrt (ux * ux + uy * uy) * sqrt (vx * vx + vy * vy)));

  theta = ((ux * vy - uy * vx) < 0) ? 0 - theta : theta;

  theta = GCODE_RAD2DEG * theta;

  GCODE_MATH_WRAP_TO_360_DEGREES (theta);                                       // Yes, fmod (x, 360.0) DOES sometimes return "360.0". Yes, that would be BAD.
  GCODE_MATH_SNAP_TO_360_DEGREES (theta);

  ux = vx;
  uy = vy;

  vx = (x2 - cx) / r;
  vy = (y2 - cy) / r;

  sweep = acos ((ux * vx + uy * vy) / (sqrt (ux * ux + uy * uy) * sqrt (vx * vx + vy * vy)));

  sweep = ((ux * vy - uy * vx) < 0) ? 0 - sweep : sweep;

  sweep = GCODE_RAD2DEG * sweep;

  if (fls == 0)
  {
    if (sweep > 0)
      sweep -= 360.0;
  }
  else
  {
    if (sweep < 0)
      sweep += 360.0;
  }

  arc->radius = r;
  arc->start_angle = theta;
  arc->sweep_angle = sweep;

  return (0);
}
