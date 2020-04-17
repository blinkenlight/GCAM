/**
 *  gcode_gerber.c
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

#include "gcode_gerber.h"
#include "gcode_sketch.h"
#include "gcode_arc.h"
#include "gcode_line.h"
#include "gcode_util.h"
#include "gcode.h"

#define GERBER_PASS_1     0
#define GERBER_PASS_2     1
#define GERBER_PASS_3     2
#define GERBER_PASS_4     3
#define GERBER_PASS_5     4
#define GERBER_PASS_6     5
#define GERBER_PASS_7     6
#define GERBER_PASS_8     7
#define GERBER_PASSES     8

#define GERBER_EPSILON    GCODE_PRECISION / 10

/**
 * Convert the [0.0 ... 1.0] progress fraction of a specific Gerber pass into 
 * a [0.0 ... 1.0] progress fraction relevant to the total number of passes;
 */

#define GERBER_PROGRESS(_pass, _prog) \
  (((gfloat_t)_pass + _prog) / (gfloat_t)GERBER_PASSES)

/**
 * Assuming 'x' is the point closest to point '_p3' on the line given by points
 * '_p1' and '_p2', find the ratio '_u' between the lengths of the segment '_p1'
 * to 'x' and the segment '_p1' to '_p2' - or, to put it differently, find the
 * scalar '_u' that vector '_p1' -> '_p2' needs to be multiplied with to obtain
 * the vector whose tip 'x' is the projection of '_p3' onto the original vector.
 *
 * Adapted from Paul Bourke - October 1988
 */

#define SOLVE_U(_p1, _p2, _p3, _u) { \
  gfloat_t _dist = (_p1[0]-_p2[0])*(_p1[0]-_p2[0]) + (_p1[1]-_p2[1])*(_p1[1]-_p2[1]); \
  _u = ((_p3[0]-_p1[0])*(_p2[0]-_p1[0]) + (_p3[1]-_p1[1])*(_p2[1]-_p1[1])) / _dist; }

/**
 * The compare function used by qsort when sorting intersection points further
 * on below - the idea is that the actual point data is in the first two items
 * of a 3D vector and the third item holds the value we want to sort by, which
 * can be the distance of each point to something, its angle along an arc etc.
 */

static int
qsort_compare (const void *a, const void *b)
{
  gcode_vec3d_t *v0, *v1;

  v0 = (gcode_vec3d_t *)a;
  v1 = (gcode_vec3d_t *)b;

  if (GCODE_MATH_IS_EQUAL ((*v0)[2], (*v1)[2]))
    return (0);

  return ((*v0)[2] < (*v1)[2] ? -1 : 1);
}

/**
 * Insert an aperture into the "aperture table", but only if it's a NEW one
 */

static int
insert_aperture (int *aperture_count, gcode_gerber_aperture_t **aperture_set, uint8_t type, uint8_t index, gfloat_t width, gfloat_t height)
{
  int i;

  for (i = 0; i < *aperture_count; i++)
    if ((*aperture_set)[i].ind == index)
      if ((*aperture_set)[i].type == type)
        if (GCODE_MATH_IS_EQUAL ((*aperture_set)[i].v[0], width))
          if (GCODE_MATH_IS_EQUAL ((*aperture_set)[i].v[1], height))
            return (1);

  *aperture_set = realloc (*aperture_set, (*aperture_count + 1) * sizeof (gcode_gerber_aperture_t));

  (*aperture_set)[*aperture_count].type = type;
  (*aperture_set)[*aperture_count].ind = index;
  (*aperture_set)[*aperture_count].v[0] = width;
  (*aperture_set)[*aperture_count].v[1] = height;

  (*aperture_count)++;

  return (0);
}

/**
 * Insert an exposure spot into the "exposure table", but only if it's a NEW one
 */

static int
insert_exposure (int *exposure_count, gcode_gerber_exposure_t **exposure_set, gcode_gerber_aperture_t *aperture, gcode_vec2d_t p)
{
  int i;

  for (i = 0; i < *exposure_count; i++)
    if (GCODE_MATH_IS_EQUAL ((*exposure_set)[i].v[0], aperture->v[0]))
      if (GCODE_MATH_IS_EQUAL ((*exposure_set)[i].v[1], aperture->v[1]))
        if (GCODE_MATH_2D_DISTANCE ((*exposure_set)[i].pos, p) < GCODE_PRECISION)
          return (1);

  *exposure_set = realloc (*exposure_set, (*exposure_count + 1) * sizeof (gcode_gerber_exposure_t));

  (*exposure_set)[*exposure_count].type = aperture->type;
  (*exposure_set)[*exposure_count].v[0] = aperture->v[0];
  (*exposure_set)[*exposure_count].v[1] = aperture->v[1];
  (*exposure_set)[*exposure_count].pos[0] = p[0];
  (*exposure_set)[*exposure_count].pos[1] = p[1];

  (*exposure_count)++;

  return (0);
}

/**
 * Insert a line trace into the "trace table", but only if it's a NEW one
 */

static int
insert_trace_line (int *trace_count, gcode_gerber_trace_t **trace_set, gcode_gerber_aperture_t *aperture, gcode_vec2d_t p0, gcode_vec2d_t p1)
{
  int i;
  gfloat_t dist0, dist1;

  for (i = 0; i < *trace_count; i++)
  {
    if ((*trace_set)[i].type == GCODE_GERBER_TRACE_TYPE_LINE)
    {
      dist0 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p0, p0);
      dist1 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p1, p1);

      if ((dist0 < GCODE_PRECISION) && (dist1 < GCODE_PRECISION))
        return (1);

      dist0 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p0, p1);
      dist1 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p1, p0);

      if ((dist0 < GCODE_PRECISION) && (dist1 < GCODE_PRECISION))
        return (1);
    }
  }

  *trace_set = realloc (*trace_set, (*trace_count + 1) * sizeof (gcode_gerber_trace_t));

  (*trace_set)[*trace_count].type = GCODE_GERBER_TRACE_TYPE_LINE;

  (*trace_set)[*trace_count].p0[0] = p0[0];
  (*trace_set)[*trace_count].p0[1] = p0[1];
  (*trace_set)[*trace_count].p1[0] = p1[0];
  (*trace_set)[*trace_count].p1[1] = p1[1];

  (*trace_set)[*trace_count].width = aperture->v[0];

  (*trace_count)++;

  return (0);
}

/**
 * Insert an arc trace into the "trace table", but only if it's a NEW one
 */

static int
insert_trace_arc (int *trace_count, gcode_gerber_trace_t **trace_set, gcode_gerber_aperture_t *aperture, gcode_vec2d_t p0, gcode_vec2d_t p1, gcode_vec2d_t center_offset, int direction)
{
  int i;
  gcode_vec2d_t cp;
  gcode_arcdata_t arcdata;
  gfloat_t dist0, dist1, dist2;

  GCODE_MATH_VEC2D_COPY (arcdata.p0, p0);
  GCODE_MATH_VEC2D_COPY (arcdata.p1, p1);

  GCODE_MATH_VEC2D_MAG (arcdata.radius, center_offset);

  arcdata.fla = 0;
  arcdata.fls = (direction == GCODE_GERBER_ARC_CCW) ? 1 : 0;

  if (gcode_arc_radius_to_sweep (&arcdata) != 0)
    return (1);

  cp[0] = p0[0] - arcdata.radius * cos (arcdata.start_angle * GCODE_DEG2RAD);
  cp[1] = p0[1] - arcdata.radius * sin (arcdata.start_angle * GCODE_DEG2RAD);

  for (i = 0; i < *trace_count; i++)
  {
    if ((*trace_set)[i].type == GCODE_GERBER_TRACE_TYPE_ARC)
    {
      dist0 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].cp, cp);

      dist1 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p0, p0);
      dist2 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p1, p1);

      if ((dist0 < GCODE_PRECISION) && (dist1 < GCODE_PRECISION) && (dist2 < GCODE_PRECISION))
        return (1);

      dist1 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p0, p1);
      dist2 = GCODE_MATH_2D_DISTANCE ((*trace_set)[i].p1, p0);

      if ((dist0 < GCODE_PRECISION) && (dist1 < GCODE_PRECISION) && (dist2 < GCODE_PRECISION))
        return (1);
    }
  }

  *trace_set = realloc (*trace_set, (*trace_count + 1) * sizeof (gcode_gerber_trace_t));

  (*trace_set)[*trace_count].type = GCODE_GERBER_TRACE_TYPE_ARC;

  (*trace_set)[*trace_count].p0[0] = p0[0];
  (*trace_set)[*trace_count].p0[1] = p0[1];
  (*trace_set)[*trace_count].p1[0] = p1[0];
  (*trace_set)[*trace_count].p1[1] = p1[1];
  (*trace_set)[*trace_count].cp[0] = cp[0];
  (*trace_set)[*trace_count].cp[1] = cp[1];

  (*trace_set)[*trace_count].start_angle = arcdata.start_angle;
  (*trace_set)[*trace_count].sweep_angle = arcdata.sweep_angle;

  (*trace_set)[*trace_count].radius = arcdata.radius;
  (*trace_set)[*trace_count].width = aperture->v[0];

  (*trace_count)++;

  return (0);
}

/**
 * Insert an "elbow" element into the "elbow table", but only if it's a NEW one
 * NOTE: the "elbow table" stores the position of every single point where a
 * trace segment starts or ends, offering a way to "round up" all those joints,
 * considering the trace segments themselves are inserted without any "endcaps"
 */

static int
insert_trace_elbow (int *trace_elbow_count, gcode_vec3d_t **trace_elbow_set, gcode_gerber_aperture_t *aperture, gcode_vec2d_t p)
{
  int i;

  for (i = 0; i < *trace_elbow_count; i++)
    if (GCODE_MATH_IS_EQUAL ((*trace_elbow_set)[i][2], aperture->v[0]))
      if (GCODE_MATH_2D_DISTANCE ((*trace_elbow_set)[i], p) < GCODE_PRECISION)
        return (1);

  *trace_elbow_set = realloc (*trace_elbow_set, (*trace_elbow_count + 1) * sizeof (gcode_vec3d_t));

  (*trace_elbow_set)[*trace_elbow_count][0] = p[0];
  (*trace_elbow_set)[*trace_elbow_count][1] = p[1];
  (*trace_elbow_set)[*trace_elbow_count][2] = aperture->v[0];

  (*trace_elbow_count)++;

  return (0);
}

/**
 * Returns "TRUE" (non-zero) if 'point' is within (NOT on) the circle having
 * the center at 'center' and the diameter (NOT the radius) of 'diameter';
 */

static int
point_inside_circle (gcode_vec2d_t point, gcode_vec2d_t center, gfloat_t diameter)
{
  gfloat_t eps;

  eps = GERBER_EPSILON;

  if (GCODE_MATH_2D_DISTANCE (point, center) < 0.5 * diameter - eps)
    return (1);

  return (0);
}

/**
 * Returns "TRUE" (non-zero) if 'point' is within (NOT on) the rectangle having
 * the center at 'center' and the width / height of 'width' & 'height';
 */

static int
point_inside_rectangle (gcode_vec2d_t point, gcode_vec2d_t center, gfloat_t width, gfloat_t height)
{
  gfloat_t eps;

  eps = GERBER_EPSILON;

  if (GCODE_MATH_1D_DISTANCE (point[0], center[0]) < 0.5 * width - eps)
    if (GCODE_MATH_1D_DISTANCE (point[1], center[1]) < 0.5 * height - eps)
      return (1);

  return (0);
}

/**
 * Returns "TRUE" (non-zero) if 'point' is within (NOT on) the "obround" (two
 * semicircles connected by parallel lines tangent to their endpoints) having
 * the center at 'center' and the width / height of 'width' & 'height';
 */

static int
point_inside_obround (gcode_vec2d_t point, gcode_vec2d_t center, gfloat_t width, gfloat_t height)
{
  if (width > height)
  {
    gcode_vec2d_t center1, center2;

    center1[0] = center[0] - (width - height) * 0.5;
    center1[1] = center[1];

    center2[0] = center[0] + (width - height) * 0.5;
    center2[1] = center[1];

    if (point_inside_circle (point, center1, height))
      return (1);

    if (point_inside_circle (point, center2, height))
      return (1);

    if (point_inside_rectangle (point, center, width - height, height))
      return (1);
  }
  else if (height > width)
  {
    gcode_vec2d_t center1, center2;

    center1[0] = center[0];
    center1[1] = center[1] - (height - width) * 0.5;

    center2[0] = center[0];
    center2[1] = center[1] + (height - width) * 0.5;

    if (point_inside_circle (point, center1, width))
      return (1);

    if (point_inside_circle (point, center2, width))
      return (1);

    if (point_inside_rectangle (point, center, width, height - width))
      return (1);
  }
  else
  {
    if (point_inside_circle (point, center, width))
      return (1);
  }

  return (0);
}

/**
 * PASS 1 - Parse the Gerber file to create aperture, exposure and elbow tables
 * and open-ended ("endcap-less") trace outlines inserted under 'sketch_block';
 */

static int
gcode_gerber_pass1 (gcode_block_t *sketch_block, FILE *fh, int *trace_count, gcode_gerber_trace_t **trace_array, int *trace_elbow_count,
                    gcode_vec3d_t **trace_elbow_array, int *exposure_count, gcode_gerber_exposure_t **exposure_array, gfloat_t offset)
{
  gcode_t *gcode;
  gcode_sketch_t *sketch;
  char buf[10], *buffer = NULL;
  long int length, nomore, index;
  int i, j, buf_ind, inum, aperture_num, aperture_cmd, arc_dir;
  uint8_t aperture_ind, aperture_closed, trace_elbow_match;
  gcode_gerber_aperture_t *aperture_set;
  gcode_vec2d_t cur_pos = { 0.0, 0.0 };
  gcode_vec2d_t cur_ij = { 0.0, 0.0 };
  gcode_vec2d_t normal = { 0.0, 0.0 };
  gfloat_t digit_scale, unit_scale;
  gfloat_t progress;

  aperture_ind = 0;
  aperture_num = 0;
  aperture_cmd = 2;                                                             // Default command is 'aperture closed'
  aperture_set = NULL;
  aperture_closed = 1;
  digit_scale = 1.0;                                                            // Scale factor for integer-formatted X/Y coordinates
  unit_scale = 1.0;                                                             // Scale factor for cross-unit import (inches <-> mm)
  arc_dir = GCODE_GERBER_ARC_CW;

  gcode = (gcode_t *)sketch_block->gcode;

  sketch = (gcode_sketch_t *)sketch_block->pdata;

  fseek (fh, 0, SEEK_END);
  length = ftell (fh);
  fseek (fh, 0, SEEK_SET);
  buffer = malloc (length);
  nomore = fread (buffer, 1, length, fh);

  index = 0;

  while (index < nomore)
  {
    if (gcode->progress_callback)
    {
      progress = (gfloat_t)index / (gfloat_t)nomore;

      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_1, progress));
    }

    if (buffer[index] == '%')
    {
      index++;

      if (buffer[index] == 'M' && buffer[index + 1] == 'O')
      {
        index += 2;

        /* Unit conversion */
        if (buffer[index] == 'I' && buffer[index + 1] == 'N')
        {
          index += 2;

          if (sketch_block->gcode->units == GCODE_UNITS_MILLIMETER)
          {
            unit_scale *= GCODE_INCH2MM;
          }
        }
        else if (buffer[index] == 'M' && buffer[index + 1] == 'M')
        {
          index += 2;

          if (sketch_block->gcode->units == GCODE_UNITS_INCH)
          {
            unit_scale *= GCODE_MM2INCH;
          }
        }
        else
        {
          REMARK ("Unsupported Gerber units (neither inches nor millimeters)\n");
          return (1);
        }
      }
      else if (buffer[index] == 'F' && buffer[index + 1] == 'S')
      {
        index += 2;

        if (buffer[index] == 'L')
        {
          index++;

          if (buffer[index] == 'A')
          {
            index++;

            if (buffer[index] == 'X')
            {
              index += 2;

              buf[0] = buffer[index];
              buf[1] = 0;

              i = atoi (buf);

              index++;
            }
            else
            {
              REMARK ("Gerber X coordinate format definition is missing\n");
              return (1);
            }

            if (buffer[index] == 'Y')
            {
              index += 2;

              buf[0] = buffer[index];
              buf[1] = 0;

              j = atoi (buf);

              index++;
            }
            else
            {
              REMARK ("Gerber Y coordinate format definition is missing\n");
              return (1);
            }

            if (i == j)
            {
              for (i = 0; i < j; i++)
                digit_scale *= 0.1;
            }
            else
            {
              REMARK ("Gerber X and Y coordinate formats do not match (%i X decimals vs. %i Y decimals)\n", i, j);
              return (1);
            }
          }
          else
          {
            REMARK ("Unsupported Gerber coordinate format (other than 'absolute notation')\n");
            return (1);
          }
        }
        else
        {
          REMARK ("Unsupported Gerber coordinate format (other than 'omit leading zeros')\n");
          return (1);
        }
      }
      else if (buffer[index] == 'A' && buffer[index + 1] == 'D' && buffer[index + 2] == 'D')
      {
        index += 3;
        buf[0] = buffer[index];
        buf[1] = buffer[index + 1];
        buf[2] = 0;
        inum = atoi (buf);

        index += 2;

        if (buffer[index] == 'C')
        {
          gfloat_t diameter;

          index++;

          if (buffer[index] == ',')
            index++;

          buf_ind = 0;

          while (buffer[index] != '*')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;
          diameter = atof (buf) * unit_scale + 2 * offset;

          insert_aperture (&aperture_num, &aperture_set, GCODE_GERBER_APERTURE_TYPE_CIRCLE, inum, diameter, diameter);
        }
        else if (buffer[index] == 'R')
        {
          gfloat_t x, y;

          index++;

          if (buffer[index] == ',')
            index++;

          buf_ind = 0;

          while (buffer[index] != 'X')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;
          x = atof (buf) * unit_scale + 2 * offset;

          index++;                                                              /* Skip 'X' */

          buf_ind = 0;

          while (buffer[index] != '*')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;
          y = atof (buf) * unit_scale + 2 * offset;

          insert_aperture (&aperture_num, &aperture_set, GCODE_GERBER_APERTURE_TYPE_RECTANGLE, inum, x, y);
        }
        else if (buffer[index] == 'O' && buffer[index + 1] == 'C')              /* Convert Octagon pads to Circles */
        {
          gfloat_t diameter;

          index += 2;

          while (buffer[index] != ',')
            index++;

          index++;

          buf_ind = 0;

          while (buffer[index] != '*')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;
          diameter = atof (buf) * unit_scale + 2 * offset;

          insert_aperture (&aperture_num, &aperture_set, GCODE_GERBER_APERTURE_TYPE_CIRCLE, inum, diameter, diameter);
        }
        else if (buffer[index] == 'O')
        {
          gfloat_t x, y;

          index++;

          if (buffer[index] == ',')
            index++;

          buf_ind = 0;

          while (buffer[index] != 'X')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;
          x = atof (buf) * unit_scale + 2 * offset;

          index++;                                                              /* Skip 'X' */

          buf_ind = 0;

          while (buffer[index] != '*')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;
          y = atof (buf) * unit_scale + 2 * offset;

          if (GCODE_MATH_IS_EQUAL (x, y))
            insert_aperture (&aperture_num, &aperture_set, GCODE_GERBER_APERTURE_TYPE_CIRCLE, inum, x, y);
          else
            insert_aperture (&aperture_num, &aperture_set, GCODE_GERBER_APERTURE_TYPE_OBROUND, inum, x, y);
        }
        else if (buffer[index] == 'P')
        {
          REMARK ("Unsupported Gerber aperture definition (Polygon)\n");
          return (1);
        }
      }

      /* Find closing '%' */
      while (buffer[index] != '%')
        index++;

      index++;
    }
    else if (buffer[index] == 'X' || buffer[index] == 'Y' || buffer[index] == 'I' || buffer[index] == 'J' || buffer[index] == 'D')
    {
      gfloat_t pos[2];
      int xy_mask;
      int ij_mask;

      pos[0] = cur_pos[0];
      pos[1] = cur_pos[1];

      while (buffer[index] != '*')
      {
        xy_mask = 0;
        ij_mask = 0;

        if (buffer[index] == 'X')
        {
          index++;
          buf_ind = 0;

          while ((buffer[index] >= '0' && buffer[index] <= '9') || buffer[index] == '-')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;

          pos[0] = atof (buf) * digit_scale * unit_scale;
          xy_mask |= 1;
        }

        if (buffer[index] == 'Y')
        {
          index++;
          buf_ind = 0;

          while ((buffer[index] >= '0' && buffer[index] <= '9') || buffer[index] == '-')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;

          pos[1] = atof (buf) * digit_scale * unit_scale;
          xy_mask |= 2;
        }

        if (buffer[index] == 'I')                                               /* I */
        {
          index++;
          buf_ind = 0;

          while ((buffer[index] >= '0' && buffer[index] <= '9') || buffer[index] == '-')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;

          cur_ij[0] = atof (buf) * digit_scale * unit_scale;
          ij_mask |= 1;
        }

        if (buffer[index] == 'J')                                               /* J */
        {
          index++;
          buf_ind = 0;

          while ((buffer[index] >= '0' && buffer[index] <= '9') || buffer[index] == '-')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;

          cur_ij[1] = atof (buf) * digit_scale * unit_scale;
          ij_mask |= 2;
        }

        if (buffer[index] == 'D')                                               /* Set aperture Number or cmd */
        {
          int d;

          index++;                                                              /* skip 'D' */
          buf[0] = buffer[index];
          buf[1] = buffer[index + 1];
          buf[2] = 0;
          index += 2;

          d = atoi (buf);

          if (d > 3)
          {
            for (i = 0; i < aperture_num; i++)
              if (aperture_set[i].ind == d)
                aperture_ind = i;
          }
          else
          {
            aperture_cmd = d;
          }
        }

        if (ij_mask)
        {
          gcode_arc_t *arc;
          gcode_block_t *arc_block;
          gfloat_t start_angle, sweep_angle;
          gfloat_t width, radius;

          if (insert_trace_arc (trace_count, trace_array, &aperture_set[aperture_ind], cur_pos, pos, cur_ij, arc_dir) == 0)
          {
            width = aperture_set[aperture_ind].v[0];

            radius = (*trace_array)[*trace_count - 1].radius;

            start_angle = (*trace_array)[*trace_count - 1].start_angle;
            sweep_angle = (*trace_array)[*trace_count - 1].sweep_angle;

            normal[0] = cos (start_angle * GCODE_DEG2RAD);
            normal[1] = sin (start_angle * GCODE_DEG2RAD);

            /* Arc 1 */
            gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

            gcode_append_as_listtail (sketch_block, arc_block);

            arc = (gcode_arc_t *)arc_block->pdata;

            arc->p[0] = cur_pos[0] + 0.5 * width * normal[0];
            arc->p[1] = cur_pos[1] + 0.5 * width * normal[1];
            arc->radius = radius + 0.5 * width;
            arc->start_angle = start_angle;
            arc->sweep_angle = sweep_angle;

            /* Arc 2 */
            gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

            gcode_append_as_listtail (sketch_block, arc_block);

            arc = (gcode_arc_t *)arc_block->pdata;

            arc->p[0] = cur_pos[0] - 0.5 * width * normal[0];
            arc->p[1] = cur_pos[1] - 0.5 * width * normal[1];
            arc->radius = radius - 0.5 * width;
            arc->start_angle = start_angle;
            arc->sweep_angle = sweep_angle;

            /* If the aperture was previously closed insert an elbow - check both position and diameter for duplicity */
            if (aperture_closed)
            {
              insert_trace_elbow (trace_elbow_count, trace_elbow_array, &aperture_set[aperture_ind], cur_pos);

              aperture_closed = 0;
            }

            /* Insert an elbow at the end of this trace segment - check both position and diameter for duplicity */
            insert_trace_elbow (trace_elbow_count, trace_elbow_array, &aperture_set[aperture_ind], pos);
          }
        }
        else if (xy_mask)                                                       /* And X or Y has occurred - Uses previous aperture_cmd if a new one isn't present. */
        {
          if (aperture_cmd == 1)                                                /* Open Exposure - Trace (line) */
          {
            gcode_block_t *line_block;
            gcode_line_t *line;
            gfloat_t mag, width;

            /* Store the Trace - Check for Duplicates before storing */
            if (insert_trace_line (trace_count, trace_array, &aperture_set[aperture_ind], cur_pos, pos) == 0)
            {
              /* Line 1 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line = (gcode_line_t *)line_block->pdata;

              normal[0] = cur_pos[1] - pos[1];
              normal[1] = pos[0] - cur_pos[0];
              mag = 1.0 / GCODE_MATH_2D_MAGNITUDE (normal);
              normal[0] *= mag;
              normal[1] *= mag;

              width = aperture_set[aperture_ind].v[0];

              line->p0[0] = cur_pos[0] + 0.5 * width * normal[0];
              line->p0[1] = cur_pos[1] + 0.5 * width * normal[1];
              line->p1[0] = pos[0] + 0.5 * width * normal[0];
              line->p1[1] = pos[1] + 0.5 * width * normal[1];

              /* Line 2 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line = (gcode_line_t *)line_block->pdata;

              line->p0[0] = cur_pos[0] - 0.5 * width * normal[0];
              line->p0[1] = cur_pos[1] - 0.5 * width * normal[1];
              line->p1[0] = pos[0] - 0.5 * width * normal[0];
              line->p1[1] = pos[1] - 0.5 * width * normal[1];

              /* If the aperture was previously closed insert an elbow - check both position and diameter for duplicity */
              if (aperture_closed)
              {
                insert_trace_elbow (trace_elbow_count, trace_elbow_array, &aperture_set[aperture_ind], cur_pos);

                aperture_closed = 0;
              }

              /* Insert an elbow at the end of this trace segment - check both position and diameter for duplicity */
              insert_trace_elbow (trace_elbow_count, trace_elbow_array, &aperture_set[aperture_ind], pos);
            }
          }
          else if (aperture_cmd == 2)                                           /* Aperture Closed */
          {
            aperture_closed = 1;
          }
          else if (aperture_cmd == 3)                                           /* Flash exposure */
          {
            if (aperture_set[aperture_ind].type == GCODE_GERBER_APERTURE_TYPE_CIRCLE)
            {
              gcode_block_t *arc_block;
              gcode_arc_t *arc;
              gfloat_t diameter;

              diameter = aperture_set[aperture_ind].v[0];

              /* arc 1 */
              gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, arc_block);

              arc = (gcode_arc_t *)arc_block->pdata;

              arc->radius = 0.5 * diameter;
              arc->p[0] = pos[0];
              arc->p[1] = pos[1] + arc->radius;
              arc->start_angle = 90.0;
              arc->sweep_angle = -360.0;

              insert_exposure (exposure_count, exposure_array, &aperture_set[aperture_ind], pos);
            }
            else if (aperture_set[aperture_ind].type == GCODE_GERBER_APERTURE_TYPE_RECTANGLE)
            {
              gcode_block_t *line_block;
              gcode_line_t *line;
              gfloat_t width, height;

              width = aperture_set[aperture_ind].v[0];
              height = aperture_set[aperture_ind].v[1];

              /* Line 1 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line = (gcode_line_t *)line_block->pdata;

              line->p0[0] = pos[0] - 0.5 * width;
              line->p0[1] = pos[1] + 0.5 * height;
              line->p1[0] = pos[0] + 0.5 * width;
              line->p1[1] = pos[1] + 0.5 * height;

              /* Line 2 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line = (gcode_line_t *)line_block->pdata;

              line->p0[0] = pos[0] + 0.5 * width;
              line->p0[1] = pos[1] + 0.5 * height;
              line->p1[0] = pos[0] + 0.5 * width;
              line->p1[1] = pos[1] - 0.5 * height;

              /* Line 3 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line = (gcode_line_t *)line_block->pdata;

              line->p0[0] = pos[0] + 0.5 * width;
              line->p0[1] = pos[1] - 0.5 * height;
              line->p1[0] = pos[0] - 0.5 * width;
              line->p1[1] = pos[1] - 0.5 * height;

              /* Line 4 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line = (gcode_line_t *)line_block->pdata;

              line->p0[0] = pos[0] - 0.5 * width;
              line->p0[1] = pos[1] - 0.5 * height;
              line->p1[0] = pos[0] - 0.5 * width;
              line->p1[1] = pos[1] + 0.5 * height;

              insert_exposure (exposure_count, exposure_array, &aperture_set[aperture_ind], pos);
            }
            else if (aperture_set[aperture_ind].type == GCODE_GERBER_APERTURE_TYPE_OBROUND)
            {
              gcode_block_t *line_block;
              gcode_block_t *arc_block;
              gcode_line_t *line1, *line2;
              gcode_arc_t *arc1, *arc2;
              gfloat_t width, height;

              /* arc 1 */
              gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, arc_block);

              arc1 = (gcode_arc_t *)arc_block->pdata;

              /* Line 1 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line1 = (gcode_line_t *)line_block->pdata;

              /* arc 2 */
              gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, arc_block);

              arc2 = (gcode_arc_t *)arc_block->pdata;

              /* Line 2 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

              gcode_append_as_listtail (sketch_block, line_block);

              line2 = (gcode_line_t *)line_block->pdata;

              width = aperture_set[aperture_ind].v[0];
              height = aperture_set[aperture_ind].v[1];

              if (width > height)
              {
                arc1->p[0] = pos[0] + 0.5 * (width - height);
                arc1->p[1] = pos[1] + 0.5 * height;
                arc1->start_angle = 90.0;
                arc1->sweep_angle = -180.0;
                arc1->radius = 0.5 * height;

                arc2->p[0] = pos[0] - 0.5 * (width - height);
                arc2->p[1] = pos[1] - 0.5 * height;
                arc2->start_angle = 270.0;
                arc2->sweep_angle = -180.0;
                arc2->radius = 0.5 * height;

                line1->p0[0] = pos[0] + 0.5 * (width - height);
                line1->p0[1] = pos[1] - 0.5 * height;
                line1->p1[0] = pos[0] - 0.5 * (width - height);
                line1->p1[1] = pos[1] - 0.5 * height;

                line2->p0[0] = pos[0] - 0.5 * (width - height);
                line2->p0[1] = pos[1] + 0.5 * height;
                line2->p1[0] = pos[0] + 0.5 * (width - height);
                line2->p1[1] = pos[1] + 0.5 * height;
              }
              else
              {
                arc1->p[0] = pos[0] + 0.5 * width;
                arc1->p[1] = pos[1] - 0.5 * (height - width);
                arc1->start_angle = 0.0;
                arc1->sweep_angle = -180.0;
                arc1->radius = 0.5 * width;

                arc2->p[0] = pos[0] - 0.5 * width;
                arc2->p[1] = pos[1] + 0.5 * (height - width);
                arc2->start_angle = 180.0;
                arc2->sweep_angle = -180.0;
                arc2->radius = 0.5 * width;

                line1->p0[0] = pos[0] - 0.5 * width;
                line1->p0[1] = pos[1] - 0.5 * (height - width);
                line1->p1[0] = pos[0] - 0.5 * width;
                line1->p1[1] = pos[1] + 0.5 * (height - width);

                line2->p0[0] = pos[0] + 0.5 * width;
                line2->p0[1] = pos[1] + 0.5 * (height - width);
                line2->p1[0] = pos[0] + 0.5 * width;
                line2->p1[1] = pos[1] - 0.5 * (height - width);
              }

              insert_exposure (exposure_count, exposure_array, &aperture_set[aperture_ind], pos);
            }
          }
        }

        /* Update current position */
        if (xy_mask & 1)
          cur_pos[0] = pos[0];

        if (xy_mask & 2)
          cur_pos[1] = pos[1];
      }
    }
    else if (buffer[index] == 'G')
    {
      index++;

      if (buffer[index] == '0' && buffer[index + 1] == '1')
      {
        /* Linear interpolation - Line */
        index += 2;
        /* Using current position, generate a line. */
      }
      else if (buffer[index] == '0' && buffer[index + 1] == '2')
      {
        /* Clockwise circular interpolation */
        index += 2;
        /* Using current position, generate a CW arc. */
        arc_dir = GCODE_GERBER_ARC_CW;
      }
      else if (buffer[index] == '0' && buffer[index + 1] == '3')
      {
        /* Counter Clockwise circular interpolation */
        index += 2;
        /* Using current position, generate a CCW arc. */
        arc_dir = GCODE_GERBER_ARC_CCW;
      }
      else if (buffer[index] == '0' && buffer[index + 1] == '4')
      {
        /* Ignore data block */
        index += 2;

        while (buffer[index] != '\n')
          index++;
      }
      else if (buffer[index] == '5' && buffer[index + 1] == '4')
      {
        /* Tool Prepare */
        index += 2;

        index++;                                                                /* skip 'D' */
        buf[0] = buffer[index];
        buf[1] = buffer[index + 1];
        buf[2] = 0;
        index += 2;

        for (i = 0; i < aperture_num; i++)
          if (aperture_set[i].ind == atoi (buf))
            aperture_ind = i;
      }
      else if (buffer[index] == '7' && buffer[index + 1] == '0')
      {
        /* Specify Inches - do nothing for now */
        index += 2;
      }
      else if (buffer[index] == '7' && buffer[index + 1] == '1')
      {
        /* Specify millimeters - do nothing for now */
        index += 2;
      }
      else if (buffer[index] == '7' && buffer[index + 1] == '5')
      {
        /* Enable 360 degree circular interpolation (multiquadrant) */
        index += 2;
      }
    }
    else
    {
      index++;
    }
  }

  return (0);
}

/**
 * PASS 2 - Insert "trace elbows" (full circles) at all trace segment endpoints
 */

static void
gcode_gerber_pass2 (gcode_block_t *sketch_block, int trace_elbow_count, gcode_vec3d_t *trace_elbow)
{
  gcode_t *gcode;
  gcode_block_t *arc_block;
  gcode_arc_t *arc;
  gfloat_t progress;

  gcode = (gcode_t *)sketch_block->gcode;

  for (int i = 0; i < trace_elbow_count; i++)
  {
    if (gcode->progress_callback)
    {
      progress = (gfloat_t)i / (gfloat_t)trace_elbow_count;

      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_2, progress));
    }

    gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

    gcode_append_as_listtail (sketch_block, arc_block);

    arc = (gcode_arc_t *)arc_block->pdata;

    arc->radius = 0.5 * trace_elbow[i][2];
    arc->p[0] = trace_elbow[i][0];
    arc->p[1] = trace_elbow[i][1] + arc->radius;
    arc->start_angle = 90.0;
    arc->sweep_angle = -360.0;
  }
}

/**
 * PASS 3 - Create intersections: loop through each line and arc segment trying
 * to intersect it with every other, then divide each one up into smaller lines
 * or arcs, from one endpoint through all the intersection points to the other
 * endpoint - the new segments end-to-end must match the old one they replace;
 */

static void
gcode_gerber_pass3 (gcode_block_t *sketch_block)
{
  gcode_t *gcode;
  gcode_block_t *index1_block, *index2_block;
  gcode_block_t *original_listhead;
  gcode_vec2d_t min1, max1, min2, max2;
  gcode_vec2d_t p0, p1;
  gcode_vec2d_t ip_array[2];
  gcode_vec2d_t full_ip_array[256];
  gcode_vec3d_t full_ip_sorted_array[256];
  int ip_count, full_ip_count, full_ip_sorted_count;
  int block_count, block_index;
  gfloat_t progress;

  gcode = (gcode_t *)sketch_block->gcode;

  original_listhead = sketch_block->listhead;                                   // Save a reference to the current (original) list of 'sketch_block';

  sketch_block->listhead = NULL;                                                // Detach it from 'sketch_block' so a new list can be built in its place;

  block_count = 0;

  index1_block = original_listhead;                                             // Start with the first block on the original list of 'sketch_block';

  while (index1_block)                                                          // Crawl along the list and count the blocks;
  {
    block_count++;                                                              // This may sound like a waste of time, but the progress update needs it;

    index1_block = index1_block->next;                                          // It's a small price to pay for having some feedback that GCAM didn't crash.
  }

  block_index = 0;

  index1_block = original_listhead;                                             // Start with the first block on the original list of 'sketch_block';

  while (index1_block)                                                          // Take every single block and intersect it with every other block;
  {
    if (gcode->progress_callback)                                               // Make sure there is a progress update function to call
    {
      progress = (gfloat_t)block_index / (gfloat_t)block_count;                 // Calculate the current local progress fraction;

      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_3, progress));
    }

    full_ip_count = 0;                                                          // The total number of intersections 'index1_block' has with anything else;
    full_ip_sorted_count = 0;                                                   // The total number of UNIQUE intersections 'index1_block' has;

    index1_block->ends (index1_block, p0, p1, GCODE_GET);                       // We'll need the endpoints of the scrutinized block soon, so we save them;

    gcode_util_qdbb (index1_block, min1, max1);                                 // Calculate the "quick and dirty" bounding box of 'index1_block';

    index2_block = original_listhead;                                           // Starting from the same listhead,

    while (index2_block)                                                        // loop through all the other blocks of 'sketch_block' with a second index;
    {
      if (index1_block != index2_block)                                         // Don't perform intersection against self.
      {
        gcode_util_qdbb (index2_block, min2, max2);                             // Calculate the "quick and dirty" bounding box of 'index2_block';

        if (!GCODE_MATH_IS_APART (min1, max1, min2, max2))                      // If the two bounding boxes clear each other, don't try to intersect;
        {
          if (gcode_util_intersect (index1_block, index2_block, ip_array, &ip_count) == 0)
          {
            for (int i = 0; i < ip_count; i++)                                  // Examine every intersection point returned (if any):
            {
              if (GCODE_MATH_2D_DISTANCE (p0, ip_array[i]) < GCODE_PRECISION)   // Something touching one of the endpoints of 'index1_block', while technically
                continue;                                                       // being an 'intersection', CANNOT DIVIDE THE BLOCK IN TWO, so drop that point;

              if (GCODE_MATH_2D_DISTANCE (p1, ip_array[i]) < GCODE_PRECISION)   // Same with the other endpoint - if the only intersections found coincide with
                continue;                                                       // the endpoints, THEN THERE ARE NO INTERSECTIONS as in no division is needed!

              GCODE_MATH_VEC2D_COPY (full_ip_array[full_ip_count], ip_array[i]);        // If we're here, this is a genuine intersection that will divide the block...

              full_ip_count++;                                                  // So save it into the full array and increase the total intersection count;
            }
          }
        }
      }

      index2_block = index2_block->next;                                        // Move on to the next block to attempt to intersect with 'index1_block';
    }

    if (index1_block->type == GCODE_TYPE_LINE)                                  // The division process is type-specific, so this is what we do for lines:
    {
      gcode_block_t *line_block;
      gcode_line_t *line, *new_line;
      gfloat_t distance;

      line = (gcode_line_t *)index1_block->pdata;                               // Have 'line' point to the line-specific data of the block to be divided;

      if (full_ip_count)                                                        // There were intersections...
      {
        int i, j;

        for (i = 0; i < full_ip_count; i++)                                     // Loop through each intersection point,
        {
          distance = GCODE_MATH_2D_DISTANCE (p0, full_ip_array[i]);             // and find out how far is is from p0 (the first endpoint of the line);

          for (j = 0; j < full_ip_sorted_count; j++)                            // Now loop through each of the ALREADY COPIED intersection points,
            if (GCODE_MATH_IS_EQUAL (full_ip_array[i][0], full_ip_sorted_array[j][0]))  // and see if there is any with the same coordinates;
              if (GCODE_MATH_IS_EQUAL (full_ip_array[i][1], full_ip_sorted_array[j][1]))
                break;

          if (j == full_ip_sorted_count)                                        // If the loop ran all the way up, there were no matches;
          {
            GCODE_MATH_VEC2D_COPY (full_ip_sorted_array[j], full_ip_array[i]);  // So take the current point and copy it over into the sorted array;
            full_ip_sorted_array[j][2] = distance;                              // Add the "sorting metric" (distance from p0) as the third value;
            full_ip_sorted_count++;                                             // Increment number of unique intersection points waiting to be sorted;
          }
        }

        GCODE_MATH_VEC2D_COPY (full_ip_sorted_array[full_ip_sorted_count], p0); // Add the first endpoint of the line (p0) to the list;
        full_ip_sorted_array[full_ip_sorted_count][2] = 0;
        full_ip_sorted_count++;

        distance = GCODE_MATH_2D_DISTANCE (p0, p1);

        GCODE_MATH_VEC2D_COPY (full_ip_sorted_array[full_ip_sorted_count], p1); // Add the second endpoint of the line (p1) to the list;
        full_ip_sorted_array[full_ip_sorted_count][2] = distance;
        full_ip_sorted_count++;

        qsort (full_ip_sorted_array, full_ip_sorted_count, sizeof (gcode_vec3d_t), qsort_compare);

        for (int i = 0; i < full_ip_sorted_count - 1; i++)                      // Create connecting lines between each intersection point and the next one;
        {
          gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

          gcode_insert_as_listhead (sketch_block, line_block);

          new_line = (gcode_line_t *)line_block->pdata;

          GCODE_MATH_VEC2D_COPY (new_line->p0, full_ip_sorted_array[i]);
          GCODE_MATH_VEC2D_COPY (new_line->p1, full_ip_sorted_array[i + 1]);
        }
      }
      else                                                                      // If there were no intersections, copy the line verbatim to the new list;
      {
        /* Just copy the line, do nothing, no intersections. */
        gcode_line_init (&line_block, sketch_block->gcode, sketch_block);

        gcode_insert_as_listhead (sketch_block, line_block);

        new_line = (gcode_line_t *)line_block->pdata;

        GCODE_MATH_VEC2D_COPY (new_line->p0, p0);
        GCODE_MATH_VEC2D_COPY (new_line->p1, p1);
      }
    }
    else if (index1_block->type == GCODE_TYPE_ARC)                              // On the other hand, if 'index1_block' is an arc, divide it like this:
    {
      gcode_block_t *arc_block;
      gcode_arc_t *arc, *new_arc;
      gcode_vec2d_t center;
      gfloat_t angle, delta;

      arc = (gcode_arc_t *)index1_block->pdata;

      if (full_ip_count)                                                        // There were intersections...
      {
        int i, j;

        gcode_arc_center (index1_block, center, GCODE_GET);                     // We'll need the position of the center of this arc, so find it;

        for (i = 0; i < full_ip_count; i++)                                     // Loop through each intersection point,
        {
          gcode_math_xy_to_angle (center, full_ip_array[i], &angle);            // and find out what angle it is at, on the circle the arc is part of;

          if ((arc->sweep_angle > 0) && (angle < arc->start_angle))             // If the sweep is positive (CCW) but the angle is smaller than 'start_angle',
            angle += 360.0;                                                     // wrap it around CCW to make sure it's "bigger" therefore sortable;

          if ((arc->sweep_angle < 0) && (angle > arc->start_angle))             // If the sweep is negative (CW) but the angle is larger than 'start_angle',
            angle -= 360.0;                                                     // wrap it around CW to make sure it's "smaller" therefore sortable;

          delta = fabs (angle - arc->start_angle);                              // To be able to sort either CW or CCW arcs, we have to use relative angles;

          for (j = 0; j < full_ip_sorted_count; j++)                            // Now loop through each of the ALREADY COPIED intersection points,
            if (GCODE_MATH_IS_EQUAL (full_ip_array[i][0], full_ip_sorted_array[j][0]))  // and see if there is any with the same coordinates;
              if (GCODE_MATH_IS_EQUAL (full_ip_array[i][1], full_ip_sorted_array[j][1]))
                break;

          if (j == full_ip_sorted_count)                                        // If the loop ran all the way up, there were no matches;
          {
            GCODE_MATH_VEC2D_COPY (full_ip_sorted_array[j], full_ip_array[i]);  // So take the current point and copy it over into the sorted array;
            full_ip_sorted_array[j][2] = delta;                                 // Add the "sorting metric" (angle away from p0) as the third value;
            full_ip_sorted_count++;                                             // Increment number of unique intersection points waiting to be sorted;
          }
        }

        GCODE_MATH_VEC2D_COPY (full_ip_sorted_array[full_ip_sorted_count], p0); // Add the first endpoint of the arc (p0) to the list;
        full_ip_sorted_array[full_ip_sorted_count][2] = 0;
        full_ip_sorted_count++;

        delta = fabs (arc->sweep_angle);                                        // The "angle away from p0" of p1 is the sweep angle itself;

        GCODE_MATH_VEC2D_COPY (full_ip_sorted_array[full_ip_sorted_count], p1); // Add the second endpoint of the arc (p1) to the list;
        full_ip_sorted_array[full_ip_sorted_count][2] = delta;
        full_ip_sorted_count++;

        qsort (full_ip_sorted_array, full_ip_sorted_count, sizeof (gcode_vec3d_t), qsort_compare);

        for (i = 0; i < full_ip_sorted_count - 1; i++)                          // Create connecting arcs between each intersection point and the next one;
        {
          gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

          gcode_insert_as_listhead (sketch_block, arc_block);

          new_arc = (gcode_arc_t *)arc_block->pdata;

          GCODE_MATH_VEC2D_COPY (new_arc->p, full_ip_sorted_array[i]);          // The starting point of the arc is trivial: one of the intersection points;

          new_arc->radius = arc->radius;                                        // The radius is the same as that of the original arc;

          if (arc->sweep_angle > 0)                                             // The start angle is "sort value" away from the start angle of the original;
            new_arc->start_angle = arc->start_angle + full_ip_sorted_array[i][2];
          else
            new_arc->start_angle = arc->start_angle - full_ip_sorted_array[i][2];

          GCODE_MATH_WRAP_TO_360_DEGREES (new_arc->start_angle);                // As always, the start angle must be wrapped back to within [0...360);
          GCODE_MATH_SNAP_TO_360_DEGREES (new_arc->start_angle);

          if (arc->sweep_angle > 0)                                             // The sweep angle is simply the difference between the two "sort values";
            new_arc->sweep_angle = full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2];
          else
            new_arc->sweep_angle = -(full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2]);
        }
      }
      else                                                                      // If there were no intersections, copy the arc verbatim to the new list;
      {
        /* Just copy the arc, do nothing, no intersections. */
        gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);

        gcode_insert_as_listhead (sketch_block, arc_block);

        new_arc = (gcode_arc_t *)arc_block->pdata;

        GCODE_MATH_VEC2D_COPY (new_arc->p, p0);

        new_arc->radius = arc->radius;
        new_arc->start_angle = arc->start_angle;
        new_arc->sweep_angle = arc->sweep_angle;
      }
    }

    block_index++;                                                              // Count the processed blocks for the progress update;

    index1_block = index1_block->next;                                          // Move on to the next block in the original list of 'sketch_block';
  }

  gcode_list_free (&original_listhead);                                         // Free the original list of 'sketch_block', it's no longer needed;
}

/**
 * PASS 4 - Eliminate internal intersections: out of all those partial segments
 * created in pass 3, remove all that fall within a trace or a within a pad;
 */

static void
gcode_gerber_pass4 (gcode_block_t *sketch_block, int trace_count, gcode_gerber_trace_t *trace_array, int exposure_count, gcode_gerber_exposure_t *exposure_array)
{
  gcode_t *gcode;
  gcode_block_t *index1_block, *index2_block;
  gcode_block_t *line_block, *arc_block;
  gcode_line_t *line;
  gcode_arc_t *arc;
  int block_count, block_index, remove_block;
  gfloat_t progress;
  gfloat_t eps;

  eps = GERBER_EPSILON;

  gcode = (gcode_t *)sketch_block->gcode;

  gcode_line_init (&line_block, gcode, NULL);
  gcode_arc_init (&arc_block, gcode, NULL);

  line = (gcode_line_t *)line_block->pdata;
  arc = (gcode_arc_t *)arc_block->pdata;

  block_count = 0;

  index1_block = sketch_block->listhead;                                        // Start with the first block on the list of 'sketch_block';

  while (index1_block)                                                          // Crawl along the list and count the blocks;
  {
    block_count++;                                                              // This may sound like a waste of time, but the progress update needs it;

    index1_block = index1_block->next;                                          // It's a small price to pay for having some feedback that GCAM didn't crash.
  }

  block_index = 0;

  index1_block = sketch_block->listhead;

  while (index1_block)
  {
    gcode_vec2d_t bmin, bmax;
    gcode_vec2d_t p0, p1, midp;

    if (gcode->progress_callback)                                               // Make sure there is a progress update function to call
    {
      progress = (gfloat_t)block_index / (gfloat_t)block_count;                 // Calculate the current local progress fraction;

      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_4, progress));
    }

    index1_block->ends (index1_block, p0, p1, GCODE_GET);                       // Calculate the endpoints of 'index1_block";

    switch (index1_block->type)
    {
      case GCODE_TYPE_LINE:

        gcode_line_qdbb (index1_block, bmin, bmax);                             // Calculate the "quick and dirty" bounding box of 'index1_block' as a line;
        gcode_line_midpoint (index1_block, midp, GCODE_GET);                    // Calculate 'midp' for 'index1_block' as the point halfway between its ends;

        break;

      case GCODE_TYPE_ARC:
      {
        gcode_arc_qdbb (index1_block, bmin, bmax);                              // Calculate the "quick and dirty" bounding box of 'index1_block' as an arc;
        gcode_arc_midpoint (index1_block, midp, GCODE_GET);                     // Calculate 'midp' for 'index1_block' as the point on the arc at half sweep;

        break;
      }
    }

    remove_block = 0;                                                           // Preset the "remove flag" to "do not remove";

    /**
     * Trace Interference Check
     */

    for (int i = 0; i < trace_count && !remove_block; i++)                      // Loop through each trace in the trace array;
    {
      gcode_vec2d_t ip_array[2], dpos;
      gcode_vec2d_t tmin, tmax;
      gfloat_t trace_radius;
      gfloat_t dist, u;
      gfloat_t angle;
      int ip_count;

      trace_radius = 0.5 * trace_array[i].width;

      switch (trace_array[i].type)
      {
        case GCODE_GERBER_TRACE_TYPE_LINE:

          GCODE_MATH_VEC2D_COPY (line->p0, trace_array[i].p0);                  // Copy the trace into 'line_block' for clearance checks;
          GCODE_MATH_VEC2D_COPY (line->p1, trace_array[i].p1);

          gcode_line_qdbb (line_block, tmin, tmax);                             // Calculate the "quick and dirty" bounding box of the trace;

          /* Intersect Test 0 - is it intersecting the trace centerline */

          if (!GCODE_MATH_IS_APART (bmin, bmax, tmin, tmax))                    // If the bounding boxes clear each other, don't try to intersect;
            if (gcode_util_intersect (line_block, index1_block, ip_array, &ip_count) == 0)
              remove_block = 1;

          /* Intersect Test 1 - does endpoint 0 fall within trace footprint */

          SOLVE_U (line->p0, line->p1, p0, u);                                  // Find the ratio 'u' yielding the projection of 'p0' onto 'line';

          if (u < 0.0)                                                          // If the projection would fall outside the segment [p0, p1], clamp 'u';
            u = 0.0;

          if (u > 1.0)                                                          // If the projection would fall outside the segment [p0, p1], clamp 'u';
            u = 1.0;

          dpos[0] = line->p0[0] + u * (line->p1[0] - line->p0[0]);              // Calculate the actual projection point 'dpos' as given by 'u';
          dpos[1] = line->p0[1] + u * (line->p1[1] - line->p0[1]);

          dist = GCODE_MATH_2D_DISTANCE (dpos, p0);                             // See how far the endpoint 'p0' of the block is from 'dpos';

          if (dist < trace_radius - eps)                                        // If it's closer than the trace radius, intruder alert...!
            remove_block = 1;

          /* Intersect Test 2 - does endpoint 1 fall within trace footprint */

          SOLVE_U (line->p0, line->p1, p1, u);                                  // Find the ratio 'u' yielding the projection of 'p1' onto 'line';

          if (u < 0.0)                                                          // If the projection would fall outside the segment [p0, p1], clamp 'u';
            u = 0.0;

          if (u > 1.0)                                                          // If the projection would fall outside the segment [p0, p1], clamp 'u';
            u = 1.0;

          dpos[0] = line->p0[0] + u * (line->p1[0] - line->p0[0]);              // Calculate the actual projection point 'dpos' as given by 'u';
          dpos[1] = line->p0[1] + u * (line->p1[1] - line->p0[1]);

          dist = GCODE_MATH_2D_DISTANCE (dpos, p1);                             // See how far the endpoint 'p1' of the block is from 'dpos';

          if (dist < trace_radius - eps)                                        // If it's closer than the trace radius, intruder alert...!
            remove_block = 1;

          /* Intersect Test 3 - does midpoint fall within trace footprint */

          SOLVE_U (line->p0, line->p1, midp, u);                                // Find the ratio 'u' yielding the projection of 'midp' onto 'line';

          if (u < 0.0)                                                          // If the projection would fall outside the segment [p0, p1], clamp 'u';
            u = 0.0;

          if (u > 1.0)                                                          // If the projection would fall outside the segment [p0, p1], clamp 'u';
            u = 1.0;

          dpos[0] = line->p0[0] + u * (line->p1[0] - line->p0[0]);              // Calculate the actual projection point 'dpos' as given by 'u';
          dpos[1] = line->p0[1] + u * (line->p1[1] - line->p0[1]);

          dist = GCODE_MATH_2D_DISTANCE (dpos, midp);                           // See how far the midpoint 'midp' of the arc is from 'dpos';

          if (dist < trace_radius - eps)                                        // If it's closer than the trace radius, intruder alert...!
            remove_block = 1;

          break;

        case GCODE_GERBER_TRACE_TYPE_ARC:

          GCODE_MATH_VEC2D_COPY (arc->p, trace_array[i].p0);                    // Copy the trace into 'arc_block' for clearance checks;

          arc->radius = trace_array[i].radius;

          arc->start_angle = trace_array[i].start_angle;
          arc->sweep_angle = trace_array[i].sweep_angle;

          gcode_arc_qdbb (arc_block, tmin, tmax);                               // Calculate the "quick and dirty" bounding box of the trace;

          /* Intersect Test 0 - is it intersecting the trace centerline */

          if (!GCODE_MATH_IS_APART (bmin, bmax, tmin, tmax))                    // If the bounding boxes clear each other, don't try to intersect;
            if (gcode_util_intersect (arc_block, index1_block, ip_array, &ip_count) == 0)
              remove_block = 1;

          /* Intersect Test 1 - does endpoint 0 fall within trace body */

          gcode_math_xy_to_angle (trace_array[i].cp, p0, &angle);

          dist = GCODE_MATH_2D_DISTANCE (trace_array[i].cp, p0);

          if (gcode_math_angle_within_arc (arc->start_angle, arc->sweep_angle, angle) == 0)
            if (dist < arc->radius + trace_radius - eps)
              if (dist > arc->radius - trace_radius + eps)
                remove_block = 1;

          /* Intersect Test 1.5 - does endpoint 0 fall within trace endcaps */

          if (GCODE_MATH_2D_DISTANCE (trace_array[i].p0, p0) < trace_radius - eps)
            remove_block = 1;

          if (GCODE_MATH_2D_DISTANCE (trace_array[i].p1, p0) < trace_radius - eps)
            remove_block = 1;

          /* Intersect Test 2 - does endpoint 1 fall within trace body */

          gcode_math_xy_to_angle (trace_array[i].cp, p1, &angle);

          dist = GCODE_MATH_2D_DISTANCE (trace_array[i].cp, p1);

          if (gcode_math_angle_within_arc (arc->start_angle, arc->sweep_angle, angle) == 0)
            if (dist < arc->radius + trace_radius - eps)
              if (dist > arc->radius - trace_radius + eps)
                remove_block = 1;

          /* Intersect Test 2.5 - does endpoint 1 fall within trace endcaps */

          if (GCODE_MATH_2D_DISTANCE (trace_array[i].p0, p1) < trace_radius - eps)
            remove_block = 1;

          if (GCODE_MATH_2D_DISTANCE (trace_array[i].p1, p1) < trace_radius - eps)
            remove_block = 1;

          /* Intersect Test 3 - does midpoint fall within trace body */

          gcode_math_xy_to_angle (trace_array[i].cp, midp, &angle);

          dist = GCODE_MATH_2D_DISTANCE (trace_array[i].cp, midp);

          if (gcode_math_angle_within_arc (arc->start_angle, arc->sweep_angle, angle) == 0)
            if (dist < arc->radius + trace_radius - eps)
              if (dist > arc->radius - trace_radius + eps)
                remove_block = 1;

          /* Intersect Test 3.5 - does midpoint fall within trace endcaps */

          if (GCODE_MATH_2D_DISTANCE (trace_array[i].p0, midp) < trace_radius - eps)
            remove_block = 1;

          if (GCODE_MATH_2D_DISTANCE (trace_array[i].p1, midp) < trace_radius - eps)
            remove_block = 1;

          break;
      }
    }

    /**
     * Exposure (Pad) Interference Check
     */

    for (int i = 0; i < exposure_count && !remove_block; i++)
    {
      switch (exposure_array[i].type)
      {
        case GCODE_GERBER_APERTURE_TYPE_CIRCLE:
        {
          gcode_vec2d_t center;
          gfloat_t diameter;

          center[0] = exposure_array[i].pos[0];
          center[1] = exposure_array[i].pos[1];

          diameter = exposure_array[i].v[0];

          if (point_inside_circle (p0, center, diameter))
            remove_block = 1;

          if (point_inside_circle (p1, center, diameter))
            remove_block = 1;

          if (point_inside_circle (midp, center, diameter))
            remove_block = 1;

          break;
        }

        case GCODE_GERBER_APERTURE_TYPE_RECTANGLE:
        {
          gcode_vec2d_t center;
          gfloat_t width, height;

          center[0] = exposure_array[i].pos[0];
          center[1] = exposure_array[i].pos[1];

          width = exposure_array[i].v[0];
          height = exposure_array[i].v[1];

          if (point_inside_rectangle (p0, center, width, height))
            remove_block = 1;

          if (point_inside_rectangle (p1, center, width, height))
            remove_block = 1;

          if (point_inside_rectangle (midp, center, width, height))
            remove_block = 1;

          break;
        }

        case GCODE_GERBER_APERTURE_TYPE_OBROUND:
        {
          gcode_vec2d_t center;
          gfloat_t width, height;

          center[0] = exposure_array[i].pos[0];
          center[1] = exposure_array[i].pos[1];

          width = exposure_array[i].v[0];
          height = exposure_array[i].v[1];

          if (point_inside_obround (p0, center, width, height))
            remove_block = 1;

          if (point_inside_obround (p1, center, width, height))
            remove_block = 1;

          if (point_inside_obround (midp, center, width, height))
            remove_block = 1;

          break;
        }
      }
    }

    block_index++;

    if (remove_block)
    {
      index2_block = index1_block;
      index1_block = index1_block->next;
      gcode_remove_and_destroy (index2_block);
    }
    else
    {
      index1_block = index1_block->next;
    }
  }

  line_block->free (&line_block);
  arc_block->free (&arc_block);
}

/**
 * PASS 5 - Remove exact overlaps of segments as the result of two pads/traces
 * being perfectly adjacent. If an overlap occurs only remove one of the two,
 * not both.
 */

static void
gcode_gerber_pass5 (gcode_block_t *sketch_block)
{
  gcode_t *gcode;
  gcode_block_t *index1_block, *index2_block;
  gcode_vec2d_t e0[2], e1[2];
  gfloat_t dist0, dist1;
  int block_count, block_index, match;
  gfloat_t progress;

  gcode = (gcode_t *)sketch_block->gcode;

  block_count = 0;

  index1_block = sketch_block->listhead;                                        // Start with the first block on the list of 'sketch_block';

  while (index1_block)                                                          // Crawl along the list and count the blocks;
  {
    block_count++;                                                              // This may sound like a waste of time, but the progress update needs it;

    index1_block = index1_block->next;                                          // It's a small price to pay for having some feedback that GCAM didn't crash.
  }

  block_index = 0;

  index1_block = sketch_block->listhead;                                        // Start with the first block on the list of 'sketch_block';

  while (index1_block)                                                          // Take every single block and compare it with every other block;
  {
    if (gcode->progress_callback)                                               // Make sure there is a progress update function to call
    {
      progress = (gfloat_t)block_index / (gfloat_t)block_count;                 // Calculate the current local progress fraction;

      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_5, progress));
    }

    index1_block->ends (index1_block, e0[0], e0[1], GCODE_GET);

    match = 0;

    index2_block = index1_block->next;

    while (index2_block && !match)
    {
      index2_block->ends (index2_block, e1[0], e1[1], GCODE_GET);

      dist0 = GCODE_MATH_2D_DISTANCE (e0[0], e1[0]);
      dist1 = GCODE_MATH_2D_DISTANCE (e0[1], e1[1]);

      if ((dist0 < GCODE_PRECISION) && (dist1 < GCODE_PRECISION))
        match = 1;

      dist0 = GCODE_MATH_2D_DISTANCE (e0[1], e1[0]);
      dist1 = GCODE_MATH_2D_DISTANCE (e0[0], e1[1]);

      if ((dist0 < GCODE_PRECISION) && (dist1 < GCODE_PRECISION))
        match = 1;

      /* Remove block2 */
      if (match)
      {
        gcode_remove_and_destroy (index2_block);
      }
      else
      {
        index2_block = index2_block->next;
      }
    }

    block_index++;

    index1_block = index1_block->next;
  }
}

/**
 * PASS 6 - Correct the orientation and sequence (continuity) of all segments.
 */

static void
gcode_gerber_pass6 (gcode_block_t *sketch_block)
{
  gcode_t *gcode;
  gfloat_t progress;

  gcode = (gcode_t *)sketch_block->gcode;

  gcode_util_merge_list_fragments (&sketch_block->listhead);

  /* Cheating like crazy, but 'merge' doesn't have progress feedback... */

  if (gcode->progress_callback)
    for (progress = 0.0; progress < 1.0; progress += 0.01)
      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_6, progress));
}

/**
 * PASS 7 - Merge adjacent lines with matching slopes.
 */

static void
gcode_gerber_pass7 (gcode_block_t *sketch_block)
{
  gcode_t *gcode;
  gcode_block_t *index1_block, *index2_block;
  gcode_vec2d_t v0, v1, e0[2], e1[2];
  int block_count, block_index, merge_block;
  gfloat_t progress;

  gcode = (gcode_t *)sketch_block->gcode;

  if (!sketch_block->listhead)                                                  // Not much to merge in an empty list, innit...
    return;

  block_count = 0;

  index1_block = sketch_block->listhead;                                        // Start with the first block on the list of 'sketch_block';

  while (index1_block)                                                          // Crawl along the list and count the blocks;
  {
    block_count++;                                                              // This may sound like a waste of time, but the progress update needs it;

    index1_block = index1_block->next;                                          // It's a small price to pay for having some feedback that GCAM didn't crash.
  }

  block_index = 0;

  index1_block = sketch_block->listhead;                                        // Start with the first and the second block in the list;
  index2_block = index1_block->next;

  while (index1_block && index2_block)                                          // As long as both blocks exist, keep looping;
  {
    if (gcode->progress_callback)                                               // Make sure there is a progress update function to call
    {
      progress = (gfloat_t)block_index / (gfloat_t)block_count;                 // Calculate the current local progress fraction;

      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_7, progress));
    }

    merge_block = 0;                                                            // Clear the merge flag for a new round;

    /* Check that both blocks are actually lines */

    if ((index1_block->type == GCODE_TYPE_LINE) && (index2_block->type == GCODE_TYPE_LINE))
    {
      index1_block->ends (index1_block, e0[0], e0[1], GCODE_GET);               // Get the endpoints of line 1, then their difference (the slope of line 1)
      GCODE_MATH_VEC2D_SUB (v0, e0[1], e0[0]);

      index2_block->ends (index2_block, e1[0], e1[1], GCODE_GET);               // Get the endpoints of line 2, then their difference (the slope of line 2)
      GCODE_MATH_VEC2D_SUB (v1, e1[1], e1[0]);

      /* Make sure the points are connected */

      if (GCODE_MATH_IS_EQUAL (e0[1][0], e1[0][0]) && GCODE_MATH_IS_EQUAL (e0[1][1], e1[0][1]))
      {
        /* Check for matching slopes */

        if ((fabs (v0[1]) < GCODE_PRECISION) && (fabs (v1[1]) < GCODE_PRECISION))
        {
          merge_block = 1;                                                      // Both lines are horizontal -> merge;
        }
        else if ((fabs (v0[0]) < GCODE_PRECISION) && (fabs (v1[0]) < GCODE_PRECISION))
        {
          merge_block = 1;                                                      // Both lines are vertical -> merge;
        }
        else if ((fabs (v0[0]) < GCODE_PRECISION) || (fabs (v1[0]) < GCODE_PRECISION))
        {
          merge_block = 0;                                                      // Not a match, but we really should avoid DIVIDING BY ZERO;
        }
        else if (fabs (v0[1] / v0[0] - v1[1] / v1[0]) < GCODE_PRECISION)
        {
          merge_block = 1;                                                      // Both lines have the same slope -> merge;
        }

        if (merge_block)                                                        // Merging just means the end of line 1 changes to the end of line 2;
        {
          gcode_line_t *line;

          line = (gcode_line_t *)index1_block->pdata;

          GCODE_MATH_VEC2D_COPY (line->p1, e1[1]);
        }
      }
    }

    if (merge_block)                                                            // If we merged, get rid of block 2 and restart at the beginning of the list;
    {
      gcode_remove_and_destroy (index2_block);

      block_count--;
      block_index = 0;

      index1_block = sketch_block->listhead;
    }
    else                                                                        // Otherwise just crawl along the list by one block;
    {
      block_index++;

      index1_block = index2_block;
    }

    index2_block = index1_block->next;                                          // Either way, block 2 is always the one following block 1;
  }
}

/**
 * PASS 8 - Merge adjacent arcs with matching centers.
 */

static void
gcode_gerber_pass8 (gcode_block_t *sketch_block)
{
  gcode_t *gcode;
  gcode_block_t *index1_block, *index2_block;
  gcode_arc_t *arc1, *arc2;
  gcode_vec2d_t c1, c2, b1[2], b2[2], pb;
  int block_count, block_index, merge_block;
  gfloat_t progress;
  gfloat_t angle;

  gcode = (gcode_t *)sketch_block->gcode;

  if (!sketch_block->listhead)                                                  // Not much to merge in an empty list, innit...
    return;

  block_count = 0;

  index1_block = sketch_block->listhead;                                        // Start with the first block on the list of 'sketch_block';

  while (index1_block)                                                          // Crawl along the list and count the blocks;
  {
    block_count++;                                                              // This may sound like a waste of time, but the progress update needs it;

    index1_block = index1_block->next;                                          // It's a small price to pay for having some feedback that GCAM didn't crash.
  }

  block_index = 0;

  index1_block = sketch_block->listhead;                                        // Start with the first block in the list;

  while (index1_block)
  {
    if (gcode->progress_callback)                                               // Make sure there is a progress update function to call
    {
      progress = (gfloat_t)block_index / (gfloat_t)block_count;                 // Calculate the current local progress fraction;

      gcode->progress_callback (gcode->gui, GERBER_PROGRESS (GERBER_PASS_8, progress));
    }

    merge_block = 0;                                                            // Preset the merge flag to 'no merge';

    if (index1_block->type == GCODE_TYPE_ARC)                                   // If the current block ('index1') is not an arc, skip it;
    {
      arc1 = (gcode_arc_t *)index1_block->pdata;

      gcode_arc_center (index1_block, c1, GCODE_GET);                           // Obtain the center and endpoints of 'index1' as 'c1' and 'b1[]';

      index1_block->ends (index1_block, b1[0], b1[1], GCODE_GET);

      GCODE_MATH_VEC2D_COPY (pb, b1[1]);                                        // Set the current "end of" endpoint ('pb') to the end of 'index1';

      index2_block = index1_block->next;                                        // Start iterating from the block following 'index1' (if any);

      while (index2_block)                                                      // Loop through blocks one by one, crawling away from 'index1';
      {
        index2_block->ends (index2_block, b2[0], b2[1], GCODE_GET);             // Obtain the endpoints of the iterator block ('index2');

        if (GCODE_MATH_2D_DISTANCE (pb, b2[0]) >= GCODE_PRECISION)              // If 'index2' is NOT connected to the block before it (which ends at 'pb'),
          break;                                                                // this is a new contour, there won't be a match for 'index1': stop looking.

        if (index2_block->type == GCODE_TYPE_ARC)                               // If we're within the same contour as 'index1', see if 'index2' is an arc;
        {
          arc2 = (gcode_arc_t *)index2_block->pdata;

          gcode_arc_center (index2_block, c2, GCODE_GET);                       // Obtain the center and endpoints of 'index2' as 'c2' and 'b2[]';

          if (GCODE_MATH_2D_DISTANCE (c1, c2) < GCODE_PRECISION)                // Merging can only happen if the arcs have the same center;
          {
            angle = arc1->sweep_angle + arc2->sweep_angle;                      // Also, consecutive arcs with a combined sweep bigger than full circle,

            if (fabs (angle) <= 360.0)                                          // no matter how unlikely they are in a Gerber contour, cannot be merged;
            {
              if (GCODE_MATH_2D_DISTANCE (b1[1], b2[0]) < GCODE_PRECISION)      // If 'index2' starts right where 'index1' ends, we may merge them;
              {
                arc1->sweep_angle = angle;                                      // The sweep of 'index1' is simply replaced with the combined sweep;

                merge_block = 1;                                                // The 'merge flag' gets set to avoid moving 'index1' after the loop;

                block_count--;                                                  // The number of blocks (for progress bar purposes) is now one less;

                gcode_remove_and_destroy (index2_block);                        // Finally 'index2' is removed and disposed of;

                break;                                                          // Stop iterating, but the next round restarts with the same 'index1'.
              }
              else if (GCODE_MATH_2D_DISTANCE (b2[1], b1[0]) < GCODE_PRECISION) // If 'index2' ENDS right where 'index1' STARTS, it's a reverse merge;
              {
                arc2->sweep_angle = angle;                                      // The sweep of 'index2' is the one to become the combined sweep;

                merge_block = 1;                                                // The 'merge flag' gets set to avoid moving 'index1' after the loop;

                block_count--;                                                  // The number of blocks (for progress bar purposes) is now one less;

                index2_block = index1_block;                                    // This time, the block to be removed is 'index1' itself;
                index1_block = index1_block->next;                              // So move on to the block next to it while 'index1' still exists,

                gcode_remove_and_destroy (index2_block);                        // then take the saved reference to 'index1' and remove the block;

                break;                                                          // Stop iterating, and the next round starts with the new 'index1'.
              }
            }
          }
        }

        GCODE_MATH_VEC2D_COPY (pb, b2[1]);                                      // If we're still iterating, the new "end of" is now the end of 'index2';

        index2_block = index2_block->next;                                      // Time to move on the the next block after 'index2';
      }
    }

    if (!merge_block)                                                           // If we stopped iterating with 'index2' and not because of a merge,
    {
      block_index++;                                                            // there were no matches found for 'index1' to merge with, so move on;

      index1_block = index1_block->next;                                        // Increment the progress block count and point 'index1' to the next block.
    }
  }
}

/**
 * Main Gerber import routine - read 'filename', call all processing passes
 * and return the resulting contours inserted under the supplied 'sketch_block'
 * NOTE: the supplied sketch will see its extrusion depth set to 'depth', with
 * a single pass (since resolution also equals 'depth');
 * NOTE: the generated contour is 'offset' amount "larger" than the precise
 * Gerber outline itself: as the first pass offset normally equals tool radius;
 * NOTE: as tempting as it looks, this is NOT a general-purpose algorithm that
 * can enlarge/shrink arbitrary outlines that we could use for, say, pocketing
 * too - it relies on knowledge derived from the Gerber trace/pad "skeleton" 
 * inside the generated contour to decide what gets removed and what remains.
 */

int
gcode_gerber_import (gcode_block_t *sketch_block, char *filename, gfloat_t depth, gfloat_t offset)
{
  FILE *fh;
  gcode_t *gcode;
  int trace_elbow_count, trace_count, exposure_count, error;
  gcode_extrusion_t *extrusion;
  gcode_line_t *line;
  gcode_vec3d_t *trace_elbow_array = NULL;
  gcode_gerber_trace_t *trace_array = NULL;
  gcode_gerber_exposure_t *exposure_array = NULL;

  fh = fopen (filename, "r");

  if (!fh)
    return (1);

  gcode = (gcode_t *)sketch_block->gcode;

  extrusion = (gcode_extrusion_t *)sketch_block->extruder->pdata;

  extrusion->resolution = depth;

  line = (gcode_line_t *)sketch_block->extruder->listhead->pdata;

  line->p1[1] = -depth;

  sprintf (sketch_block->comment, "Pass offset: %.4f", offset);                 // Note the offset for this pass in the sketch comment;

  extrusion->cut_side = GCODE_EXTRUSION_ALONG;

  trace_elbow_count = 0;
  trace_count = 0;
  exposure_count = 0;

  if (gcode->progress_callback)                                                 // Clean up the progress bar before we begin;
    gcode->progress_callback (gcode->gui, 0.0);

  error = gcode_gerber_pass1 (sketch_block, fh, &trace_count, &trace_array, &trace_elbow_count, &trace_elbow_array, &exposure_count, &exposure_array, offset);

  fclose (fh);

  if (!error)                                                                   // Only execute further passes if there was no error during the first;
  {
    gcode_gerber_pass2 (sketch_block, trace_elbow_count, trace_elbow_array);
    gcode_gerber_pass3 (sketch_block);
    gcode_gerber_pass4 (sketch_block, trace_count, trace_array, exposure_count, exposure_array);
    gcode_gerber_pass5 (sketch_block);
    gcode_gerber_pass6 (sketch_block);
    gcode_gerber_pass7 (sketch_block);
    gcode_gerber_pass8 (sketch_block);
  }

  free (trace_array);
  free (trace_elbow_array);
  free (exposure_array);

  if (gcode->progress_callback)                                                 // Clean up the progress bar before we leave;
    gcode->progress_callback (gcode->gui, 0.0);

  return (error);
}
