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

#define GCODE_GERBER_ARC_CCW  0
#define GCODE_GERBER_ARC_CW   1

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

static void
insert_trace_elbow (int *trace_elbow_num, gcode_vec3d_t **trace_elbow, uint8_t aperture_ind, gcode_gerber_aperture_t *aperture_set, gcode_vec2d_t pos)
{
  int i;
  uint8_t trace_elbow_match;

  trace_elbow_match = 0;

  for (i = 0; i < *trace_elbow_num; i++)
    if ((GCODE_MATH_2D_DISTANCE ((*trace_elbow)[i], pos) < GCODE_PRECISION) &&
        GCODE_MATH_IS_EQUAL ((*trace_elbow)[i][2], aperture_set[aperture_ind].v[0]))
      trace_elbow_match = 1;

  if (!trace_elbow_match)
  {
    *trace_elbow = (gcode_vec3d_t *)realloc (*trace_elbow, (*trace_elbow_num + 1) * sizeof (gcode_vec3d_t));
    (*trace_elbow)[*trace_elbow_num][0] = pos[0];
    (*trace_elbow)[*trace_elbow_num][1] = pos[1];
    (*trace_elbow)[*trace_elbow_num][2] = aperture_set[aperture_ind].v[0];
    (*trace_elbow_num)++;
  }
}

static int
gcode_gerber_pass1 (gcode_block_t *sketch_block, FILE *fh, int *trace_num, gcode_gerber_trace_t **trace_array, int *trace_elbow_num,
                    gcode_vec3d_t **trace_elbow, int *exposure_num, gcode_gerber_exposure_t **exposure_array, gfloat_t offset)
{
  gcode_sketch_t *sketch;
  char buf[10], *buffer = NULL;
  long int length, nomore, index;
  int i, j, buf_ind, inum, aperture_num, aperture_cmd, arc_dir;
  uint8_t aperture_ind = 0, aperture_closed, trace_elbow_match;
  gcode_gerber_aperture_t *aperture_set;
  gcode_vec2d_t cur_pos = { 0.0, 0.0 };
  gcode_vec2d_t cur_ij = { 0.0, 0.0 };
  gcode_vec2d_t normal = { 0.0, 0.0 };
  gfloat_t digit_scale, unit_scale;

  aperture_num = 0;
  aperture_cmd = 2;                                                             // Default command is 'aperture closed'
  aperture_set = NULL;
  aperture_closed = 1;
  digit_scale = 1.0;                                                            // Scale factor for integer-formatted X/Y coordinates
  unit_scale = 1.0;                                                             // Scale factor for cross-unit import (inches <-> mm)
  arc_dir = GCODE_GERBER_ARC_CW;

  sketch = (gcode_sketch_t *)sketch_block->pdata;

  fseek (fh, 0, SEEK_END);
  length = ftell (fh);
  fseek (fh, 0, SEEK_SET);
  buffer = (char *)malloc (length);
  nomore = fread (buffer, 1, length, fh);

  index = 0;

  while (index < nomore)
  {
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
          diameter = atof (buf) * unit_scale;

          aperture_set = (gcode_gerber_aperture_t *)realloc (aperture_set, (aperture_num + 1) * sizeof (gcode_gerber_aperture_t));
          aperture_set[aperture_num].type = GCODE_GERBER_APERTURE_TYPE_CIRCLE;
          aperture_set[aperture_num].ind = inum;
          aperture_set[aperture_num].v[0] = diameter + 2 * offset;
          aperture_num++;
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
          x = atof (buf) * unit_scale;

          index++;                                                              /* Skip 'X' */

          buf_ind = 0;

          while (buffer[index] != '*')
          {
            buf[buf_ind] = buffer[index];
            buf_ind++;
            index++;
          }

          buf[buf_ind] = 0;
          y = atof (buf) * unit_scale;

          aperture_set = (gcode_gerber_aperture_t *)realloc (aperture_set, (aperture_num + 1) * sizeof (gcode_gerber_aperture_t));
          aperture_set[aperture_num].type = GCODE_GERBER_APERTURE_TYPE_RECTANGLE;
          aperture_set[aperture_num].ind = inum;
          aperture_set[aperture_num].v[0] = x + 2 * offset;
          aperture_set[aperture_num].v[1] = y + 2 * offset;
          aperture_num++;
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
          diameter = atof (buf) * unit_scale;

          aperture_set = (gcode_gerber_aperture_t *)realloc (aperture_set, (aperture_num + 1) * sizeof (gcode_gerber_aperture_t));
          aperture_set[aperture_num].type = GCODE_GERBER_APERTURE_TYPE_CIRCLE;
          aperture_set[aperture_num].ind = inum;
          aperture_set[aperture_num].v[0] = diameter + 2 * offset;
          aperture_num++;
        }
        else if (buffer[index] == 'O')
        {
          REMARK ("Unsupported Gerber aperture definition (Obround)\n");
          return (1);
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
    else if (buffer[index] == 'X' || buffer[index] == 'Y' || buffer[index] == 'I' || buffer[index] == 'J')
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
          gcode_vec2d_t center;
          gfloat_t radius, start_angle, end_angle, sweep_angle;

          /* Calculate arc radius */
          GCODE_MATH_VEC2D_MAG (radius, cur_ij);

          /**
           * Use the tangent of the previous trace to determine start angle of arc.
           * Use the tangent of the previous trace to calculate the normal to space the arcs apart.
           * Starting point of arc is simply current position.
           * NOTES:
           * - An elbow may need to be inserted into the list after an arc.
           * - Store a previous normal.
           * - Make sure that cur_pos gets updated when a G02/G03 occures, not just after a line.
           * - Look into whether or not these arcs should exist in the trace list (duplicity etc).
           */

          /* Calculate start angle and sweep angle based on current position and destination. */
          GCODE_MATH_VEC2D_ADD (center, cur_ij, cur_pos);

          gcode_math_xy_to_angle (center, cur_pos, &start_angle);
          gcode_math_xy_to_angle (center, pos, &end_angle);

          if (end_angle < start_angle)
            end_angle += 360.0;

          if (arc_dir == GCODE_GERBER_ARC_CW)
          {
            sweep_angle = start_angle - end_angle;
          }
          else if (arc_dir == GCODE_GERBER_ARC_CCW)
          {
            if (end_angle < start_angle)
              end_angle += 360.0;

            sweep_angle = end_angle - start_angle;
          }

          /* Arc 1 */
          gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);
          arc_block->offset = &sketch->offset;
          arc_block->prev = NULL;
          arc_block->next = NULL;
          gcode_list_insert (&sketch_block->listhead, arc_block);

          arc = (gcode_arc_t *)arc_block->pdata;
          arc->p[0] = cur_pos[0] + 0.5 * normal[0] * aperture_set[aperture_ind].v[0];
          arc->p[1] = cur_pos[1] + 0.5 * normal[1] * aperture_set[aperture_ind].v[0];
          arc->start_angle = start_angle;
          arc->sweep_angle = sweep_angle;
          arc->radius = radius;

          /* Arc 2 */
          gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);
          arc_block->offset = &sketch->offset;
          arc_block->prev = NULL;
          arc_block->next = NULL;
          gcode_list_insert (&sketch_block->listhead, arc_block);

          arc = (gcode_arc_t *)arc_block->pdata;
          arc->p[0] = cur_pos[0] - 0.5 * normal[0] * aperture_set[aperture_ind].v[0];
          arc->p[1] = cur_pos[1] - 0.5 * normal[1] * aperture_set[aperture_ind].v[0];
          arc->start_angle = start_angle;
          arc->sweep_angle = sweep_angle;
          arc->radius = radius;

          insert_trace_elbow (trace_elbow_num, trace_elbow, aperture_ind, aperture_set, pos);
        }
        else if (xy_mask)                                                       /* And X or Y has occured - Uses previous aperture_cmd if a new one isn't present. */
        {
          if (aperture_cmd == 1)                                                /* Open Exposure - Trace (line) */
          {
            gcode_block_t *line_block;
            gcode_line_t *line;
            gfloat_t mag, dist0, dist1;
            int duplicate_trace;

            /* Store the Trace - Check for Duplicates before storing */
            duplicate_trace = 0;

            for (i = 0; i < *trace_num; i++)
            {
              dist0 = GCODE_MATH_2D_DISTANCE ((*trace_array)[i].p0, cur_pos);
              dist1 = GCODE_MATH_2D_DISTANCE ((*trace_array)[i].p1, pos);

              if (dist0 < GCODE_PRECISION && dist1 < GCODE_PRECISION)
                duplicate_trace = 1;

              dist0 = GCODE_MATH_2D_DISTANCE ((*trace_array)[i].p0, pos);
              dist1 = GCODE_MATH_2D_DISTANCE ((*trace_array)[i].p1, cur_pos);

              if (dist0 < GCODE_PRECISION && dist1 < GCODE_PRECISION)
                duplicate_trace = 1;
            }

            normal[0] = cur_pos[1] - pos[1];
            normal[1] = pos[0] - cur_pos[0];
            mag = 1.0 / GCODE_MATH_2D_MAGNITUDE (normal);
            normal[0] *= mag;
            normal[1] *= mag;

            if (!duplicate_trace)
            {
              *trace_array = (gcode_gerber_trace_t *)realloc (*trace_array, (*trace_num + 1) * sizeof (gcode_gerber_trace_t));
              (*trace_array)[*trace_num].p0[0] = cur_pos[0];
              (*trace_array)[*trace_num].p0[1] = cur_pos[1];
              (*trace_array)[*trace_num].p1[0] = pos[0];
              (*trace_array)[*trace_num].p1[1] = pos[1];
              (*trace_array)[*trace_num].radius = 0.5 * aperture_set[aperture_ind].v[0];
              (*trace_num)++;

              /* Line 1 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
              line_block->offset = &sketch->offset;
              line_block->prev = NULL;
              line_block->next = NULL;
              gcode_list_insert (&sketch_block->listhead, line_block);

              line = (gcode_line_t *)line_block->pdata;
              line->p0[0] = cur_pos[0] + 0.5 * normal[0] * aperture_set[aperture_ind].v[0];
              line->p0[1] = cur_pos[1] + 0.5 * normal[1] * aperture_set[aperture_ind].v[0];
              line->p1[0] = pos[0] + 0.5 * normal[0] * aperture_set[aperture_ind].v[0];
              line->p1[1] = pos[1] + 0.5 * normal[1] * aperture_set[aperture_ind].v[0];

              /* Line 2 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
              line_block->offset = &sketch->offset;
              line_block->prev = NULL;
              line_block->next = NULL;
              gcode_list_insert (&sketch_block->listhead, line_block);

              line = (gcode_line_t *)line_block->pdata;
              line->p0[0] = cur_pos[0] - 0.5 * normal[0] * aperture_set[aperture_ind].v[0];
              line->p0[1] = cur_pos[1] - 0.5 * normal[1] * aperture_set[aperture_ind].v[0];
              line->p1[0] = pos[0] - 0.5 * normal[0] * aperture_set[aperture_ind].v[0];
              line->p1[1] = pos[1] - 0.5 * normal[1] * aperture_set[aperture_ind].v[0];

              /* If the aperture was previously closed insert an elbow - check both position and diameter for duplicity */
              if (aperture_closed)
              {
                insert_trace_elbow (trace_elbow_num, trace_elbow, aperture_ind, aperture_set, cur_pos);

                aperture_closed = 0;
              }

              /* Insert an elbow at the end of this trace segment - check both position and diameter for duplicity */
              insert_trace_elbow (trace_elbow_num, trace_elbow, aperture_ind, aperture_set, pos);
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

              /* arc 1 */
              gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);
              arc_block->offset = &sketch->offset;
              arc_block->prev = NULL;
              arc_block->next = NULL;
              gcode_list_insert (&sketch_block->listhead, arc_block);

              arc = (gcode_arc_t *)arc_block->pdata;
              arc->p[0] = pos[0] - 0.5 * aperture_set[aperture_ind].v[0];
              arc->p[1] = pos[1];
              arc->start_angle = 180.0;
              arc->sweep_angle = -360.0;
              arc->radius = 0.5 * aperture_set[aperture_ind].v[0];

              *exposure_array = (gcode_gerber_exposure_t *)realloc (*exposure_array, (*exposure_num + 1) * sizeof (gcode_gerber_exposure_t));
              (*exposure_array)[*exposure_num].type = GCODE_GERBER_APERTURE_TYPE_CIRCLE;
              (*exposure_array)[*exposure_num].pos[0] = pos[0];
              (*exposure_array)[*exposure_num].pos[1] = pos[1];
              (*exposure_array)[*exposure_num].v[0] = aperture_set[aperture_ind].v[0];
              (*exposure_num)++;
            }
            else if (aperture_set[aperture_ind].type == GCODE_GERBER_APERTURE_TYPE_RECTANGLE)
            {
              gcode_block_t *line_block;
              gcode_line_t *line;

              /* Line 1 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
              line_block->offset = &sketch->offset;
              line_block->prev = NULL;
              line_block->next = NULL;
              gcode_list_insert (&sketch_block->listhead, line_block);

              line = (gcode_line_t *)line_block->pdata;
              line->p0[0] = pos[0] - 0.5 * aperture_set[aperture_ind].v[0];
              line->p0[1] = pos[1] + 0.5 * aperture_set[aperture_ind].v[1];
              line->p1[0] = pos[0] + 0.5 * aperture_set[aperture_ind].v[0];
              line->p1[1] = pos[1] + 0.5 * aperture_set[aperture_ind].v[1];

              /* Line 2 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
              line_block->offset = &sketch->offset;
              line_block->prev = NULL;
              line_block->next = NULL;
              gcode_list_insert (&sketch_block->listhead, line_block);

              line = (gcode_line_t *)line_block->pdata;
              line->p0[0] = pos[0] + 0.5 * aperture_set[aperture_ind].v[0];
              line->p0[1] = pos[1] + 0.5 * aperture_set[aperture_ind].v[1];
              line->p1[0] = pos[0] + 0.5 * aperture_set[aperture_ind].v[0];
              line->p1[1] = pos[1] - 0.5 * aperture_set[aperture_ind].v[1];

              /* Line 3 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
              line_block->offset = &sketch->offset;
              line_block->prev = NULL;
              line_block->next = NULL;
              gcode_list_insert (&sketch_block->listhead, line_block);

              line = (gcode_line_t *)line_block->pdata;
              line->p0[0] = pos[0] + 0.5 * aperture_set[aperture_ind].v[0];
              line->p0[1] = pos[1] - 0.5 * aperture_set[aperture_ind].v[1];
              line->p1[0] = pos[0] - 0.5 * aperture_set[aperture_ind].v[0];
              line->p1[1] = pos[1] - 0.5 * aperture_set[aperture_ind].v[1];

              /* Line 4 */
              gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
              line_block->offset = &sketch->offset;
              line_block->prev = NULL;
              line_block->next = NULL;
              gcode_list_insert (&sketch_block->listhead, line_block);

              line = (gcode_line_t *)line_block->pdata;
              line->p0[0] = pos[0] - 0.5 * aperture_set[aperture_ind].v[0];
              line->p0[1] = pos[1] - 0.5 * aperture_set[aperture_ind].v[1];
              line->p1[0] = pos[0] - 0.5 * aperture_set[aperture_ind].v[0];
              line->p1[1] = pos[1] + 0.5 * aperture_set[aperture_ind].v[1];

              *exposure_array = (gcode_gerber_exposure_t *)realloc (*exposure_array, (*exposure_num + 1) * sizeof (gcode_gerber_exposure_t));
              (*exposure_array)[*exposure_num].type = GCODE_GERBER_APERTURE_TYPE_RECTANGLE;
              (*exposure_array)[*exposure_num].pos[0] = pos[0];
              (*exposure_array)[*exposure_num].pos[1] = pos[1];
              (*exposure_array)[*exposure_num].v[0] = aperture_set[aperture_ind].v[0];
              (*exposure_array)[*exposure_num].v[1] = aperture_set[aperture_ind].v[1];
              (*exposure_num)++;
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

static void
gcode_gerber_pass2 (gcode_block_t *sketch_block, int trace_elbow_num, gcode_vec3d_t *trace_elbow)
{
  /**
   * PASS 2
   * Insert Trace Elbows
   */
  gcode_block_t *arc_block;
  gcode_sketch_t *sketch;
  gcode_arc_t *arc;
  int i;

  sketch = (gcode_sketch_t *)sketch_block->pdata;

  for (i = 0; i < trace_elbow_num; i++)
  {
    /* Circle Transition */
    gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);
    arc_block->offset = &sketch->offset;
    arc_block->prev = NULL;
    arc_block->next = NULL;
    gcode_list_insert (&sketch_block->listhead, arc_block);

    arc = (gcode_arc_t *)arc_block->pdata;
    arc->radius = 0.5 * trace_elbow[i][2];
    arc->p[0] = trace_elbow[i][0] - arc->radius;
    arc->p[1] = trace_elbow[i][1];
    arc->start_angle = 180;
    arc->sweep_angle = -360.0;
  }
}

static void
gcode_gerber_pass3 (gcode_block_t *sketch_block)
{
  /**
   * PASS 3 - Create Intersections.
   * Loop through each line and arc and segment for each intersection.
   */
  gcode_block_t *index1_block, *index2_block, *intersection_listhead = NULL;
  gcode_sketch_t *sketch;
  gcode_vec3d_t full_ip_sorted_array[256];
  gcode_vec2d_t full_ip_array[256], ip_array[2];
  int i, full_ip_num, ip_num;

  sketch = (gcode_sketch_t *)sketch_block->pdata;

  index1_block = sketch_block->listhead;

  while (index1_block)
  {
    full_ip_num = 0;

    index2_block = sketch_block->listhead;

    while (index2_block)
    {
      if (index1_block != index2_block)                                         /* Don't perform intersection against self. */
      {
        if (!gcode_util_intersect (index1_block, index2_block, ip_array, &ip_num))
        {
          for (i = 0; i < ip_num; i++)
          {
            full_ip_array[full_ip_num][0] = ip_array[i][0];
            full_ip_array[full_ip_num][1] = ip_array[i][1];
            full_ip_num++;
          }
        }
      }

      index2_block = index2_block->next;
    }

    if (index1_block->type == GCODE_TYPE_LINE)
    {
      gcode_block_t *line_block;
      gcode_line_t *line, *new_line;

      line = (gcode_line_t *)index1_block->pdata;

      /**
       * Generate line segments in order from line->p0 to p1 by sorting
       * the full_ip_num based on distance from line->p0.
       */
      if (full_ip_num)
      {
        /* There were intersections. */
        for (i = 0; i < full_ip_num; i++)
        {
          full_ip_sorted_array[i][0] = full_ip_array[i][0];
          full_ip_sorted_array[i][1] = full_ip_array[i][1];
          full_ip_sorted_array[i][2] = GCODE_MATH_2D_DISTANCE (line->p0, full_ip_array[i]);
        }
        qsort (full_ip_sorted_array, full_ip_num, sizeof (gcode_vec3d_t), qsort_compare);

        /* First point to first segment */
        if (full_ip_sorted_array[0][2] > GCODE_PRECISION)
        {
          gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
          line_block->offset = &sketch->offset;
          line_block->prev = NULL;
          line_block->next = NULL;
          gcode_list_insert (&intersection_listhead, line_block);

          new_line = (gcode_line_t *)line_block->pdata;
          new_line->p0[0] = line->p0[0];
          new_line->p0[1] = line->p0[1];
          new_line->p1[0] = full_ip_sorted_array[0][0];
          new_line->p1[1] = full_ip_sorted_array[0][1];
        }

        /* Generate a line segment from full_ip_sorted_array[n] to full_ip_sorted_array[n+1] */
        for (i = 0; i < full_ip_num - 1; i++)
        {
          if (full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2] > GCODE_PRECISION)
          {
            gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
            line_block->offset = &sketch->offset;
            line_block->prev = NULL;
            line_block->next = NULL;
            gcode_list_insert (&intersection_listhead, line_block);

            new_line = (gcode_line_t *)line_block->pdata;
            new_line->p0[0] = full_ip_sorted_array[i][0];
            new_line->p0[1] = full_ip_sorted_array[i][1];
            new_line->p1[0] = full_ip_sorted_array[i + 1][0];
            new_line->p1[1] = full_ip_sorted_array[i + 1][1];
          }
        }

        /* Last segment to last point */
        if (GCODE_MATH_2D_DISTANCE (line->p1, full_ip_sorted_array[full_ip_num - 1]) > GCODE_PRECISION)
        {
          gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
          line_block->offset = &sketch->offset;
          line_block->prev = NULL;
          line_block->next = NULL;
          gcode_list_insert (&intersection_listhead, line_block);

          new_line = (gcode_line_t *)line_block->pdata;
          new_line->p0[0] = full_ip_sorted_array[full_ip_num - 1][0];
          new_line->p0[1] = full_ip_sorted_array[full_ip_num - 1][1];
          new_line->p1[0] = line->p1[0];
          new_line->p1[1] = line->p1[1];
        }
      }
      else
      {
        /* Just copy the line, do nothing, no intersections. */
        gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
        line_block->offset = &sketch->offset;
        line_block->prev = NULL;
        line_block->next = NULL;
        gcode_list_insert (&intersection_listhead, line_block);

        new_line = (gcode_line_t *)line_block->pdata;
        new_line->p0[0] = line->p0[0];
        new_line->p0[1] = line->p0[1];
        new_line->p1[0] = line->p1[0];
        new_line->p1[1] = line->p1[1];
      }
    }
    else if (index1_block->type == GCODE_TYPE_ARC)
    {
      /* XXX - ARCS exist either as traces or elbows */
      gcode_block_t *arc_block;
      gcode_arc_t *arc, *new_arc;
      gcode_vec2d_t center;
      gfloat_t angle;

      arc = (gcode_arc_t *)index1_block->pdata;

      if (!full_ip_num)
      {
        /* Just copy the arc, do nothing, no intersections. */
        gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);
        arc_block->offset = &sketch->offset;
        arc_block->prev = NULL;
        arc_block->next = NULL;
        gcode_list_insert (&intersection_listhead, arc_block);

        new_arc = (gcode_arc_t *)arc_block->pdata;
        new_arc->p[0] = arc->p[0];
        new_arc->p[1] = arc->p[1];
        new_arc->start_angle = arc->start_angle;
        new_arc->sweep_angle = arc->sweep_angle;
        new_arc->radius = arc->radius;
      }
      else
      {
        gcode_vec2d_t origin, p0, span, test_span;
        gfloat_t radius_offset, mid;

        /* There were intersections. */
        gcode_arc_with_offset (index1_block, origin, center, p0, &radius_offset, &angle);

        for (i = 0; i < full_ip_num; i++)
        {
          gcode_math_xy_to_angle (center, full_ip_array[i], &angle);

          full_ip_sorted_array[i][0] = full_ip_array[i][0];
          full_ip_sorted_array[i][1] = full_ip_array[i][1];
          full_ip_sorted_array[i][2] = angle;
        }
        qsort (full_ip_sorted_array, full_ip_num, sizeof (gcode_vec3d_t), qsort_compare);

        /* 
         * You have the original start angle and sweep, therefore you can prevent arcs from being formed outside that region.
         * original arc is: "arc"
         */
        for (i = 0; i < full_ip_num - 1; i++)
        {
          /* Make sure the arc is big enough */
          if (arc->radius > GCODE_PRECISION && full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2] > GCODE_ANGULAR_PRECISION)
          {
            if (arc->sweep_angle > 0.0)
            {
              span[0] = arc->start_angle;
              span[1] = arc->start_angle + arc->sweep_angle;
            }
            else
            {
              span[0] = arc->start_angle + arc->sweep_angle;
              span[1] = arc->start_angle;
            }

            if (full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2] > 0.0)
            {
              test_span[0] = full_ip_sorted_array[i][2];
              test_span[1] = full_ip_sorted_array[i][2] + (full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2]);
            }
            else
            {
              test_span[0] = full_ip_sorted_array[i][2] + (full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2]);
              test_span[1] = full_ip_sorted_array[i][2];
            }

            mid = (test_span[0] + test_span[1]) * 0.5;

            /* Make sure that new arc falls within the existing arcs region. */
            if ((((test_span[0] >= span[0] - GCODE_ANGULAR_PRECISION - 360.0) && (test_span[0] <= span[1] + GCODE_ANGULAR_PRECISION - 360.0)) ||
                 ((test_span[0] >= span[0] - GCODE_ANGULAR_PRECISION) && (test_span[0] <= span[1] + GCODE_ANGULAR_PRECISION)) ||
                 ((test_span[0] >= span[0] - GCODE_ANGULAR_PRECISION + 360.0) && (test_span[0] <= span[1] + GCODE_ANGULAR_PRECISION + 360.0))) &&
                (((test_span[1] >= span[0] - GCODE_ANGULAR_PRECISION - 360.0) && (test_span[1] <= span[1] + GCODE_ANGULAR_PRECISION - 360.0)) ||
                 ((test_span[1] >= span[0] - GCODE_ANGULAR_PRECISION) && (test_span[1] <= span[1] + GCODE_ANGULAR_PRECISION)) ||
                 ((test_span[1] >= span[0] - GCODE_ANGULAR_PRECISION + 360.0) && (test_span[1] <= span[1] + GCODE_ANGULAR_PRECISION + 360.0))) &&
                (((mid >= span[0] - GCODE_ANGULAR_PRECISION - 360.0) && (mid <= span[1] + GCODE_ANGULAR_PRECISION - 360.0)) ||
                 ((mid >= span[0] - GCODE_ANGULAR_PRECISION) && (mid <= span[1] + GCODE_ANGULAR_PRECISION)) ||
                 ((mid >= span[0] - GCODE_ANGULAR_PRECISION + 360.0) && (mid <= span[1] + GCODE_ANGULAR_PRECISION + 360.0))))
            {
              gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);
              arc_block->offset = &sketch->offset;
              arc_block->prev = NULL;
              arc_block->next = NULL;
              gcode_list_insert (&intersection_listhead, arc_block);

              new_arc = (gcode_arc_t *)arc_block->pdata;
              new_arc->p[0] = full_ip_sorted_array[i][0];
              new_arc->p[1] = full_ip_sorted_array[i][1];
              new_arc->start_angle = full_ip_sorted_array[i][2];
              new_arc->sweep_angle = full_ip_sorted_array[i + 1][2] - full_ip_sorted_array[i][2];
              new_arc->radius = arc->radius;
            }
          }
        }

        /* Link the Last segment with the first one (if arc is a complete circle) */
        gcode_arc_init (&arc_block, sketch_block->gcode, sketch_block);
        arc_block->offset = &sketch->offset;
        arc_block->prev = NULL;
        arc_block->next = NULL;
        gcode_list_insert (&intersection_listhead, arc_block);

        new_arc = (gcode_arc_t *)arc_block->pdata;
        new_arc->p[0] = full_ip_sorted_array[full_ip_num - 1][0];
        new_arc->p[1] = full_ip_sorted_array[full_ip_num - 1][1];
        new_arc->start_angle = full_ip_sorted_array[full_ip_num - 1][2];
        new_arc->sweep_angle = 360.0 - (full_ip_sorted_array[full_ip_num - 1][2] - full_ip_sorted_array[0][2]);
        new_arc->radius = arc->radius;
      }
    }

    index1_block = index1_block->next;
  }

  gcode_remove_and_destroy (sketch_block->listhead);
  sketch_block->listhead = intersection_listhead;

  /**
   * Set parent to be the correct pointers for all blocks.
   * This will allow removal of blocks to operate on valid pointers.
   */
  index1_block = sketch_block->listhead;

  while (index1_block)
  {
    index1_block->parent = sketch_block;
    index1_block = index1_block->next;
  }
}

static void
gcode_gerber_pass4 (gcode_block_t *sketch_block, int trace_num, gcode_gerber_trace_t *trace_array, int exposure_num, gcode_gerber_exposure_t *exposure_array)
{
  /**
   * PASS 4 - Eliminate internal intersections.
   */
  gcode_block_t *line_block, *index1_block, *index2_block;
  gcode_sketch_t *sketch;
  gcode_line_t *line;
  gcode_arc_t *arc;
  gcode_vec2d_t ip_array[2], pos[2], dpos, center;
  int i, ip_num, remove;
  gfloat_t dist, u;

  sketch = (gcode_sketch_t *)sketch_block->pdata;

  gcode_line_init (&line_block, sketch_block->gcode, sketch_block);
  line_block->offset = &sketch->offset;
  line_block->prev = NULL;
  line_block->next = NULL;
  line = (gcode_line_t *)line_block->pdata;

  /**
   * Trace Intersection Check
   * Intersect the original trace with every block.
   * The key here is that the original trace runs down the center.
   */
  for (i = 0; i < trace_num; i++)
  {
    line->p0[0] = trace_array[i].p0[0];
    line->p0[1] = trace_array[i].p0[1];
    line->p1[0] = trace_array[i].p1[0];
    line->p1[1] = trace_array[i].p1[1];

    index1_block = sketch_block->listhead;

    while (index1_block)
    {
      if (!gcode_util_intersect (line_block, index1_block, ip_array, &ip_num))
      {
        /* Remove block */
        index2_block = index1_block;
        index1_block = index1_block->next;
        gcode_remove_and_destroy (index2_block);
      }
      else
      {
        index1_block = index1_block->next;
      }
    }
  }

  /* Inside Exposure (PAD) Check */
  for (i = 0; i < exposure_num; i++)
  {
    index1_block = sketch_block->listhead;

    while (index1_block)
    {
      index1_block->ends (index1_block, pos[0], pos[1], GCODE_GET);
      remove = 0;
      center[0] = 0.5 * (pos[0][0] + pos[1][0]);
      center[1] = 0.5 * (pos[0][1] + pos[1][1]);

      if (exposure_array[i].type == GCODE_GERBER_APERTURE_TYPE_CIRCLE)
      {
        dist = GCODE_MATH_2D_DISTANCE (pos[0], exposure_array[i].pos);

        if (dist < 0.5 * exposure_array[i].v[0] - GCODE_PRECISION)
          remove = 1;

        dist = GCODE_MATH_2D_DISTANCE (pos[1], exposure_array[i].pos);

        if (dist < 0.5 * exposure_array[i].v[0] - GCODE_PRECISION)
          remove = 1;

        if (index1_block->type == GCODE_TYPE_LINE)
        {
          dist = GCODE_MATH_2D_DISTANCE (center, exposure_array[i].pos);

          if (dist < 0.5 * exposure_array[i].v[0] - GCODE_PRECISION)
            remove = 1;
        }

        if (index1_block->type == GCODE_TYPE_ARC)
        {
          gcode_vec2d_t origin, arc_center, p0;
          gfloat_t arc_radius_offset, start_angle;

          arc = (gcode_arc_t *)index1_block->pdata;

          gcode_arc_with_offset (index1_block, origin, arc_center, p0, &arc_radius_offset, &start_angle);

          p0[0] = arc_center[0] + arc->radius * cos (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));
          p0[1] = arc_center[1] + arc->radius * sin (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));

          dist = GCODE_MATH_2D_DISTANCE (p0, exposure_array[i].pos);

          if (dist < 0.5 * exposure_array[i].v[0] - GCODE_PRECISION)
            remove = 1;
        }
      }
      else if (exposure_array[i].type == GCODE_GERBER_APERTURE_TYPE_RECTANGLE)
      {
        if ((pos[0][0] > exposure_array[i].pos[0] - 0.5 * exposure_array[i].v[0] + GCODE_PRECISION) &&
            (pos[0][0] < exposure_array[i].pos[0] + 0.5 * exposure_array[i].v[0] - GCODE_PRECISION) &&
            (pos[0][1] > exposure_array[i].pos[1] - 0.5 * exposure_array[i].v[1] + GCODE_PRECISION) &&
            (pos[0][1] < exposure_array[i].pos[1] + 0.5 * exposure_array[i].v[1] - GCODE_PRECISION))
          remove = 1;

        if ((pos[1][0] > exposure_array[i].pos[0] - 0.5 * exposure_array[i].v[0] + GCODE_PRECISION) &&
            (pos[1][0] < exposure_array[i].pos[0] + 0.5 * exposure_array[i].v[0] - GCODE_PRECISION) &&
            (pos[1][1] > exposure_array[i].pos[1] - 0.5 * exposure_array[i].v[1] + GCODE_PRECISION) &&
            (pos[1][1] < exposure_array[i].pos[1] + 0.5 * exposure_array[i].v[1] - GCODE_PRECISION))
          remove = 1;

        if (index1_block->type == GCODE_TYPE_LINE)
          if ((center[0] > exposure_array[i].pos[0] - 0.5 * exposure_array[i].v[0] + GCODE_PRECISION) &&
              (center[0] < exposure_array[i].pos[0] + 0.5 * exposure_array[i].v[0] - GCODE_PRECISION) &&
              (center[1] > exposure_array[i].pos[1] - 0.5 * exposure_array[i].v[1] + GCODE_PRECISION) &&
              (center[1] < exposure_array[i].pos[1] + 0.5 * exposure_array[i].v[1] - GCODE_PRECISION))
            remove = 1;

        if (index1_block->type == GCODE_TYPE_ARC)
        {
          gcode_vec2d_t origin, arc_center, p0;
          gfloat_t arc_radius_offset, start_angle;

          arc = (gcode_arc_t *)index1_block->pdata;

          gcode_arc_with_offset (index1_block, origin, arc_center, p0, &arc_radius_offset, &start_angle);

          p0[0] = arc_center[0] + arc->radius * cos (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));
          p0[1] = arc_center[1] + arc->radius * sin (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));

          if ((p0[0] > exposure_array[i].pos[0] - 0.5 * exposure_array[i].v[0] + GCODE_PRECISION) &&
              (p0[0] < exposure_array[i].pos[0] + 0.5 * exposure_array[i].v[0] - GCODE_PRECISION) &&
              (p0[1] > exposure_array[i].pos[1] - 0.5 * exposure_array[i].v[1] + GCODE_PRECISION) &&
              (p0[1] < exposure_array[i].pos[1] + 0.5 * exposure_array[i].v[1] - GCODE_PRECISION))
            remove = 1;
        }
      }

      if (remove)
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
  }

  /* Distance from Trace Check */
  for (i = 0; i < trace_num; i++)
  {
    line->p0[0] = trace_array[i].p0[0];
    line->p0[1] = trace_array[i].p0[1];
    line->p1[0] = trace_array[i].p1[0];
    line->p1[1] = trace_array[i].p1[1];

    index1_block = sketch_block->listhead;

    while (index1_block)                                                        /* xxx - add remove flag to optimize and see if there is speed improvement */
    {
      gcode_vec2d_t midpt;

      remove = 0;

      index1_block->ends (index1_block, pos[0], pos[1], GCODE_GET);

      /* Intersect Test 1 - does end pt fall within trace domain and is it less than aperture radius. */
      SOLVE_U (line->p0, line->p1, pos[0], u);

      if ((u > 0.0 + GCODE_PRECISION) && (u < 1.0 - GCODE_PRECISION))
      {
        dpos[0] = line->p0[0] + u * (line->p1[0] - line->p0[0]);
        dpos[1] = line->p0[1] + u * (line->p1[1] - line->p0[1]);
        dist = GCODE_MATH_2D_DISTANCE (dpos, pos[0]);

        if (dist < trace_array[i].radius - GCODE_PRECISION)
          remove = 1;
      }

      /* Intersect Test 2 - does end pt fall within trace domain and is it less than aperture radius. */
      SOLVE_U (line->p0, line->p1, pos[1], u);

      if ((u > 0.0 + GCODE_PRECISION) && (u < 1.0 - GCODE_PRECISION))
      {
        dpos[0] = line->p0[0] + u * (line->p1[0] - line->p0[0]);
        dpos[1] = line->p0[1] + u * (line->p1[1] - line->p0[1]);
        dist = GCODE_MATH_2D_DISTANCE (dpos, pos[1]);

        if (dist < trace_array[i].radius - GCODE_PRECISION)
          remove = 1;
      }

      /* Intersect Test 3 - Check End Points to see if they fall within the trace end radius */
      if (GCODE_MATH_2D_DISTANCE (line->p0, pos[0]) < trace_array[i].radius - GCODE_PRECISION)
        remove = 1;

      if (GCODE_MATH_2D_DISTANCE (line->p0, pos[1]) < trace_array[i].radius - GCODE_PRECISION)
        remove = 1;

      if (GCODE_MATH_2D_DISTANCE (line->p1, pos[0]) < trace_array[i].radius - GCODE_PRECISION)
        remove = 1;

      if (GCODE_MATH_2D_DISTANCE (line->p1, pos[1]) < trace_array[i].radius - GCODE_PRECISION)
        remove = 1;

      /* Intersect Test 4 - Lines only - Check if lines pass through the trace end radius */
      if (index1_block->type == GCODE_TYPE_LINE)
      {
        midpt[0] = (pos[0][0] + pos[1][0]) * 0.5;
        midpt[1] = (pos[0][1] + pos[1][1]) * 0.5;

        if (GCODE_MATH_2D_DISTANCE (line->p0, midpt) < trace_array[i].radius - GCODE_PRECISION)
          remove = 1;

        if (GCODE_MATH_2D_DISTANCE (line->p1, midpt) < trace_array[i].radius - GCODE_PRECISION)
          remove = 1;
      }

      /* Intersect Test 5 - Arcs only - Check if arcs pass through the trace */
      if (index1_block->type == GCODE_TYPE_ARC)
      {
        gcode_vec2d_t origin, center, p0;
        gfloat_t arc_radius_offset, start_angle;

        arc = (gcode_arc_t *)index1_block->pdata;

        gcode_arc_with_offset (index1_block, origin, center, p0, &arc_radius_offset, &start_angle);

        p0[0] = center[0] + arc->radius * cos (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));
        p0[1] = center[1] + arc->radius * sin (GCODE_DEG2RAD * (start_angle + arc->sweep_angle * 0.5));

        SOLVE_U (line->p0, line->p1, p0, u);                                    // Find the ratio 'u' yielding the projection of 'p0' onto 'line';

        if (u < 0.0)                                                            // If the projection would fall outside the segment [p0, p1], clamp 'u';
          u = 0.0;

        if (u > 1.0)                                                            // If the projection would fall outside the segment [p0, p1], clamp 'u';
          u = 1.0;

        dpos[0] = line->p0[0] + u * (line->p1[0] - line->p0[0]);                // Calculate the actual projection point 'dpos' as given by 'u';
        dpos[1] = line->p0[1] + u * (line->p1[1] - line->p0[1]);

        dist = GCODE_MATH_2D_DISTANCE (dpos, p0);                               // See how far the midpoint 'p0' of the arc is from 'dpos';

        if (dist < trace_array[i].radius - GCODE_PRECISION)                     // If it's closer than the trace radius, intruder alert...!
          remove = 1;
      }

      if (remove)
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
  }

  line_block->free (&line_block);
}

static void
gcode_gerber_pass5 (gcode_block_t *sketch_block)
{
  /**
   * PASS 5 - Remove exact overlaps of segments as the result of two pads or traces being perfectly adjacent.
   * If an overlap occurs only remove one of the two, not both.
   */
  gcode_block_t *index1_block, *index2_block;
  gcode_vec2d_t e0[2], e1[2];
  gfloat_t dist0, dist1;
  int match;

  index1_block = sketch_block->listhead;

  while (index1_block)
  {
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
    index1_block = index1_block->next;
  }
}

static void
gcode_gerber_pass6 (gcode_block_t *sketch_block)
{
  /**
   * PASS 6 - Correct the orientation and sequence of all segments.
   */
  gcode_util_merge_list_fragments (&sketch_block->listhead);
}

static void
gcode_gerber_pass7 (gcode_block_t *sketch_block)
{
  /**
   * PASS 7 - Merge adjacent lines with matching slopes.
   */
  gcode_block_t *index1_block, *index2_block;
  gcode_vec2d_t v0, v1, e0[2], e1[2];
  int merge;

  if (!sketch_block->listhead)                                                  // Not much to merge in an empty list, innit...
    return;

  index1_block = sketch_block->listhead;                                        // Start with the first and the second block in the list;
  index2_block = index1_block->next;

  while (index1_block && index2_block)                                          // As long as both blocks exist, keep looping;
  {
    merge = 0;                                                                  // Clear the merge flag for a new round;

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
          merge = 1;                                                            // Both lines are horizontal -> merge;
        }
        else if ((fabs (v0[0]) < GCODE_PRECISION) && (fabs (v1[0]) < GCODE_PRECISION))
        {
          merge = 1;                                                            // Both lines are vertical -> merge;
        }
        else if ((fabs (v0[0]) < GCODE_PRECISION) || (fabs (v1[0]) < GCODE_PRECISION))
        {
          merge = 0;                                                            // Not a match, but we really should avoid DIVIDING BY ZERO;
        }
        else if (fabs (v0[1] / v0[0] - v1[1] / v1[0]) < GCODE_PRECISION)
        {
          merge = 1;                                                            // Both lines have the same slope -> merge;
        }

        if (merge)                                                              // Merging just means the end of line 1 changes to the end of line 2;
        {
          gcode_line_t *line;

          line = (gcode_line_t *)index1_block->pdata;

          line->p1[0] = e1[1][0];
          line->p1[1] = e1[1][1];
        }
      }
    }

    if (merge)                                                                  // If we merged, get rid of block 2 and restart at the beginning of the list;
    {
      gcode_remove_and_destroy (index2_block);
      index1_block = sketch_block->listhead;
    }
    else                                                                        // Otherwise just crawl along the list by one block;
    {
      index1_block = index2_block;
    }

    index2_block = index1_block->next;                                          // Either way, block 2 is always the one following block 1;
  }
}

int
gcode_gerber_import (gcode_block_t *sketch_block, char *filename, gfloat_t depth, gfloat_t offset)
{
  FILE *fh;
  int trace_elbow_num, trace_num, exposure_num, error;
  gcode_extrusion_t *extrusion;
  gcode_line_t *line;
  gcode_vec3d_t *trace_elbow = NULL;
  gcode_gerber_trace_t *trace_array = NULL;
  gcode_gerber_exposure_t *exposure_array = NULL;

  fh = fopen (filename, "r");

  if (!fh)
    return (1);

  extrusion = (gcode_extrusion_t *)sketch_block->extruder->pdata;

  extrusion->resolution = depth;

  line = (gcode_line_t *)sketch_block->extruder->listhead->pdata;

  line->p1[1] = -depth;

  /* Set the comment as the file being opened */
  sprintf (sketch_block->comment, "Pass offset: %.4f", offset);

  extrusion->cut_side = GCODE_EXTRUSION_ALONG;

  trace_elbow_num = 0;
  trace_num = 0;
  exposure_num = 0;

  error = gcode_gerber_pass1 (sketch_block, fh, &trace_num, &trace_array, &trace_elbow_num, &trace_elbow, &exposure_num, &exposure_array, offset);

  if (error)
    return (1);

  gcode_gerber_pass2 (sketch_block, trace_elbow_num, trace_elbow);
  gcode_gerber_pass3 (sketch_block);
  gcode_gerber_pass4 (sketch_block, trace_num, trace_array, exposure_num, exposure_array);
  gcode_gerber_pass5 (sketch_block);
  gcode_gerber_pass6 (sketch_block);
  gcode_gerber_pass7 (sketch_block);

  free (trace_array);
  free (exposure_array);

  fclose (fh);
  return (0);
}
