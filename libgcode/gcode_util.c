/**
 *  gcode_util.c
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

#include "gcode_util.h"
#include <inttypes.h>
#include "gcode.h"
#include "gcode_arc.h"
#include "gcode_line.h"

int
gcode_util_xml_safelen (char *string)
{
  char *pscan;
  int safelen = strlen (string) + 1;

  for (pscan = string; *pscan; pscan++)
  {
    switch (*pscan)
    {
      case '<':
        safelen += 3;
        break;

      case '>':
        safelen += 3;
        break;

      case '&':
        safelen += 4;
        break;

      case '\'':
        safelen += 5;
        break;

      case '"':
        safelen += 5;
        break;
    }
  }

  return safelen;
}

void
gcode_util_xml_cpysafe (char *safestring, char *string)
{
  char *pscansrc, *pscandst;

  for (pscansrc = string, pscandst = safestring; *pscansrc; pscansrc++, pscandst++)
  {
    switch (*pscansrc)
    {
      case '<':
        strcpy (pscandst, "&lt;");
        pscandst += 3;
        break;

      case '>':
        strcpy (pscandst, "&gt;");
        pscandst += 3;
        break;

      case '&':
        strcpy (pscandst, "&amp;");
        pscandst += 4;
        break;

      case '\'':
        strcpy (pscandst, "&apos;");
        pscandst += 5;
        break;

      case '"':
        strcpy (pscandst, "&quot;");
        pscandst += 5;
        break;

      default:
        *pscandst = *pscansrc;
    }
  }

  *pscandst = '\0';
}

int
gcode_util_qsort_compare_asc (const void *a, const void *b)
{
  gfloat_t x, y;

  x = *(gfloat_t *)a;
  y = *(gfloat_t *)b;

  if (fabs (x - y) < GCODE_PRECISION)
    return (0);

  return (x < y ? -1 : 1);
}

void
gcode_util_remove_spaces (char *string)
{
  uint32_t i, n;

  i = 0;

  while (i < strlen (string))
  {
    if (string[i] == ' ')
    {
      for (n = i; n < strlen (string); n++)
      {
        string[n] = string[n + 1];
      }
    }
    else
    {
      i++;
    }
  }
}

void
gcode_util_remove_comment (char *string)
{
  uint32_t i;

  i = 0;

  while (string[i] != '\0' && string[i] != ';')
    i++;

  string[i] = 0;
}

void
gcode_util_remove_duplicate_scalars (gfloat_t *array, uint32_t *num)
{
  int32_t i, j, num2;

  num2 = *num;

  for (i = 0; i < num2 - 1; i++)
  {
    if (fabs (array[i + 1] - array[i]) < GCODE_PRECISION)
    {
      for (j = i; j < num2 - 1; j++)
      {
        array[j] = array[j + 1];
      }

      num2--;
    }
  }

  *num = num2;
}

/**
 * Calculate and return the points where a line segments and an arc intersect
 * NOTE: valid points have to actually lie between the segment's/arc's endpoints
 * NOTE: the calculations are done taking each block's offset into account
 */

static int
line_arc_intersect (gcode_block_t *line_block, gcode_block_t *arc_block, gcode_vec2d_t ip_array[2], int *ip_num)
{
  gcode_line_t *line;
  gcode_arc_t *arc;
  gcode_vec2d_t line_p0, line_p1, line_normal, arc_origin, arc_center, arc_p0, arc_p1, min, max;
  gfloat_t arc_radius, line_dx, line_dy, line_dr, line_dr_inv, line_d, line_sgn, line_disc, arc_start_angle, angle;
  int p0_test, p1_test;

  *ip_num = 0;

  line = (gcode_line_t *)line_block->pdata;
  arc = (gcode_arc_t *)arc_block->pdata;

  gcode_arc_with_offset (arc_block, arc_origin, arc_center, arc_p0, &arc_radius, &arc_start_angle);

  if (arc_radius <= GCODE_PRECISION)
    return (1);

  gcode_line_with_offset (line_block, line_p0, line_p1, line_normal);

  /**
   * Circle-Line Intersection from Wolfram MathWorld.
   * Subtract circle center from line points to represent circle center as 0,0.
   */

  line_p0[0] -= arc_center[0];
  line_p0[1] -= arc_center[1];
  line_p1[0] -= arc_center[0];
  line_p1[1] -= arc_center[1];

  line_dx = line_p1[0] - line_p0[0];
  line_dy = line_p1[1] - line_p0[1];

  line_dr = sqrt (line_dx * line_dx + line_dy * line_dy);
  line_d = line_p0[0] * line_p1[1] - line_p1[0] * line_p0[1];

  line_disc = arc_radius * arc_radius * line_dr * line_dr - line_d * line_d;

  /* Prevent floating fuzz from turning the zero discriminant into an imaginary number. */
  if ((line_disc < 0.0) && (line_disc > -GCODE_PRECISION * GCODE_PRECISION))
    line_disc = 0.0;

  if (line_disc < 0.0)
    return (1);

  line_disc = sqrt (line_disc);                                                 /* optimization */
  line_dr *= line_dr;                                                           /* optimization */
  line_dr_inv = 1.0 / line_dr;
  line_sgn = line_dy < 0.0 ? -1.0 : 1.0;

  /* Compute bounding box for line */
  if (line->p0[0] < line->p1[0])
  {
    min[0] = line->p0[0];
    max[0] = line->p1[0];
  }
  else
  {
    min[0] = line->p1[0];
    max[0] = line->p0[0];
  }

  if (line->p0[1] < line->p1[1])
  {
    min[1] = line->p0[1];
    max[1] = line->p1[1];
  }
  else
  {
    min[1] = line->p1[1];
    max[1] = line->p0[1];
  }

  min[0] -= GCODE_PRECISION;
  min[1] -= GCODE_PRECISION;
  max[0] += GCODE_PRECISION;
  max[1] += GCODE_PRECISION;

  arc_p0[0] = arc_center[0] + (line_d * line_dy + line_sgn * line_dx * line_disc) * line_dr_inv;
  arc_p0[1] = arc_center[1] + (-line_d * line_dx + fabs (line_dy) * line_disc) * line_dr_inv;

  /* Check that the point falls within the bounds of the line segment */
  p0_test = 0;

  if ((arc_p0[0] >= min[0]) && (arc_p0[0] <= max[0]) && (arc_p0[1] >= min[1]) && (arc_p0[1] <= max[1]))
  {
    gcode_math_xy_to_angle (arc_center, arc_p0, &angle);
    p0_test = gcode_math_angle_within_arc (arc_start_angle, arc->sweep_angle, angle) ? 0 : 1;
  }

  arc_p1[0] = arc_center[0] + (line_d * line_dy - line_sgn * line_dx * line_disc) * line_dr_inv;
  arc_p1[1] = arc_center[1] + (-line_d * line_dx - fabs (line_dy) * line_disc) * line_dr_inv;

  p1_test = 0;

  if ((arc_p1[0] >= min[0]) && (arc_p1[0] <= max[0]) && (arc_p1[1] >= min[1]) && (arc_p1[1] <= max[1]))
  {
    gcode_math_xy_to_angle (arc_center, arc_p1, &angle);
    p1_test = gcode_math_angle_within_arc (arc_start_angle, arc->sweep_angle, angle) ? 0 : 1;
  }

  /* Handle Tangent case where the discriminant equals 0.0 */
  if (p0_test && (line_disc > GCODE_PRECISION))
  {
    ip_array[*ip_num][0] = arc_p0[0];
    ip_array[*ip_num][1] = arc_p0[1];
    (*ip_num)++;
  }

  if (p1_test)
  {
    ip_array[*ip_num][0] = arc_p1[0];
    ip_array[*ip_num][1] = arc_p1[1];
    (*ip_num)++;
  }

  return (*ip_num ? 0 : 1);
}

/**
 * Calculate and return the points where two line segments intersect
 * NOTE: valid points have to actually lie between each segment's endpoints
 * NOTE: the calculations are done taking each block's offset into account
 */

static int
line_line_intersect (gcode_block_t *line1_block, gcode_block_t *line2_block, gcode_vec2d_t ip_array[2], int *ip_num)
{
  gcode_line_t *line1;
  gcode_line_t *line2;
  gcode_vec2d_t line1_p0, line1_p1, line1_normal, line2_p0, line2_p1, line2_normal, ip;
  gfloat_t det[4];
  gfloat_t eps;

  eps = GCODE_PRECISION;

  *ip_num = 0;

  line1 = (gcode_line_t *)line1_block->pdata;
  line2 = (gcode_line_t *)line2_block->pdata;

  gcode_line_with_offset (line1_block, line1_p0, line1_p1, line1_normal);
  gcode_line_with_offset (line2_block, line2_p0, line2_p1, line2_normal);

  if ((GCODE_MATH_2D_MANHATTAN (line2_p0, line1_p1) < eps) ||
      (GCODE_MATH_2D_MANHATTAN (line2_p0, line1_p0) < eps))
  {
    ip_array[0][0] = line2_p0[0];
    ip_array[0][1] = line2_p0[1];

    *ip_num = 1;

    return (0);                                                                 // Unless we test for it first, straight continuity gets missed as "parallels";
  }

  if ((GCODE_MATH_2D_MANHATTAN (line2_p1, line1_p1) < eps) ||
      (GCODE_MATH_2D_MANHATTAN (line2_p1, line1_p0) < eps))
  {
    ip_array[0][0] = line2_p1[0];
    ip_array[0][1] = line2_p1[1];

    *ip_num = 1;

    return (0);                                                                 // The most likely got tested first, but there are 2 x 2 possibilities...
  }

  /**
   * Line-Line Intersection from Wolfram MathWorld.
   * Solved using determinants with bound checking.
   */

  det[3] = ((line1_p0[0] - line1_p1[0]) * (line2_p0[1] - line2_p1[1])) - ((line1_p0[1] - line1_p1[1]) * (line2_p0[0] - line2_p1[0]));

  if (fabs (det[3]) < eps)                                                      // Lines may be parallel and may not intersect
    return (1);

  det[0] = line1_p0[0] * line1_p1[1] - line1_p0[1] * line1_p1[0];
  det[1] = line2_p0[0] * line2_p1[1] - line2_p0[1] * line2_p1[0];

  det[2] = (det[0] * (line2_p0[0] - line2_p1[0])) - ((line1_p0[0] - line1_p1[0]) * det[1]);
  ip_array[0][0] = ip[0] = det[2] / det[3];

  det[2] = (det[0] * (line2_p0[1] - line2_p1[1])) - ((line1_p0[1] - line1_p1[1]) * det[1]);
  ip_array[0][1] = ip[1] = det[2] / det[3];

  /* intersection point must lie within each line segment */

  if (((ip[0] < line1_p0[0] - eps) && (ip[0] < line1_p1[0] - eps)) ||
      ((ip[0] > line1_p0[0] + eps) && (ip[0] > line1_p1[0] + eps)))
    return (1);                                                                 // If (x<a and x<b) or (x>a and x>b), x cannot belong to [a b];

  if (((ip[0] < line2_p0[0] - eps) && (ip[0] < line2_p1[0] - eps)) ||
      ((ip[0] > line2_p0[0] + eps) && (ip[0] > line2_p1[0] + eps)))
    return (1);                                                                 // If (x<c and x<d) or (x>c and x>d), x cannot belong to [c d];

  /* Since ip is ON EACH LINE these are technically redundant */

  if (((ip[1] < line1_p0[1] - eps) && (ip[1] < line1_p1[1] - eps)) ||
      ((ip[1] > line1_p0[1] + eps) && (ip[1] > line1_p1[1] + eps)))
    return (1);                                                                 // If (y<a and y<b) or (y>a and y>b), y cannot belong to [a b];

  if (((ip[1] < line2_p0[1] - eps) && (ip[1] < line2_p1[1] - eps)) ||
      ((ip[1] > line2_p0[1] + eps) && (ip[1] > line2_p1[1] + eps)))
    return (1);                                                                 // If (y<c and y<d) or (y>c and y>d), y cannot belong to [c d];

  *ip_num = 1;                                                                  // Oh, still here? Ok, one intersection point it is then...

  return (0);
}

/**
 * Calculate and return the points where two arcs intersect
 * NOTE: valid points have to actually lie between each arc's endpoints
 * NOTE: the calculations are done taking each block's offset into account
 */

static int
arc_arc_intersect (gcode_block_t *arc1_block, gcode_block_t *arc2_block, gcode_vec2d_t ip_array[2], int *ip_num)
{
  gcode_arc_t *arc1;
  gcode_arc_t *arc2;
  gcode_vec2d_t arc_ip;
  gcode_vec2d_t arc1_origin, arc1_center, arc1_p0;
  gcode_vec2d_t arc2_origin, arc2_center, arc2_p0;
  gfloat_t arc1_radius, arc1_start_angle, arc2_radius, arc2_start_angle;
  gfloat_t dx, dy, d, a, h, x2, y2, rx, ry, angle1, angle2;
  int miss;

  *ip_num = 0;

  arc1 = (gcode_arc_t *)arc1_block->pdata;
  arc2 = (gcode_arc_t *)arc2_block->pdata;

  gcode_arc_with_offset (arc1_block, arc1_origin, arc1_center, arc1_p0, &arc1_radius, &arc1_start_angle);
  gcode_arc_with_offset (arc2_block, arc2_origin, arc2_center, arc2_p0, &arc2_radius, &arc2_start_angle);

  /**
   * Circle-Circle intersection code derrived from 3/26/2005 Tim Voght.
   * http://local.wasp.uwa.edu.au/~pbourke/geometry/2circle/tvoght.c
   */

  /**
   * dx and dy are the vertical and horizontal distances between the circle centers.
   */
  dx = arc2_center[0] - arc1_center[0];
  dy = arc2_center[1] - arc1_center[1];

  /* Determine the distance between the centers. */
  d = sqrt ((dy * dy) + (dx * dx));

  /* Check for solvability. */
  if (fabs (d - (arc1_radius + arc2_radius)) < GCODE_PRECISION)
    d = arc1_radius + arc2_radius;

  if (d < GCODE_PRECISION)
  {
    /* no solution.  circles overlap completely */
    return (1);
  }

  if (d > arc1_radius + arc2_radius)
  {
    /* no solution. circles do not intersect. */
    return 1;
  }

  if (d < fabs (arc1_radius - arc2_radius) - GCODE_PRECISION)
  {
    /* no solution. one circle is contained in the other */
    return 1;
  }

  /**
   * 'point 2' is the point where the line through the circle
   * intersection points crosses the line between the circle
   * centers.  
   */

  /* Determine the distance from point 0 to point 2. */
  a = ((arc1_radius * arc1_radius) - (arc2_radius * arc2_radius) + (d * d)) / (2.0 * d);

  /* Determine the coordinates of point 2. */
  x2 = arc1_center[0] + (dx * a / d);
  y2 = arc1_center[1] + (dy * a / d);

  /**
   * Determine the distance from point 2 to either of the intersection points.
   */
  h = arc1_radius * arc1_radius - a * a;

  if ((h < 0.0) && (h > -GCODE_PRECISION))
    h = 0.0;

  h = sqrt (h);

  /**
   * Now determine the offsets of the intersection points from point 2.
   */
  rx = -dy * (h / d);
  ry = dx * (h / d);

  /* Determine the absolute intersection points. */

  /**
   * If the intersection point lies within the both arcs then an intersection of the arcs has taken place.
   * There should never be 2 intersections of arcs because this would mean the sketch has bad continuity.
   */
  miss = 1;

  arc_ip[0] = x2 + rx;
  arc_ip[1] = y2 + ry;

  gcode_math_xy_to_angle (arc1_center, arc_ip, &angle1);
  gcode_math_xy_to_angle (arc2_center, arc_ip, &angle2);

  if ((gcode_math_angle_within_arc (arc1_start_angle, arc1->sweep_angle, angle1) == 0) &&
      (gcode_math_angle_within_arc (arc2_start_angle, arc2->sweep_angle, angle2) == 0))
  {
    ip_array[*ip_num][0] = arc_ip[0];
    ip_array[*ip_num][1] = arc_ip[1];
    (*ip_num)++;
    miss = 0;
  }

  arc_ip[0] = x2 - rx;
  arc_ip[1] = y2 - ry;

  gcode_math_xy_to_angle (arc1_center, arc_ip, &angle1);
  gcode_math_xy_to_angle (arc2_center, arc_ip, &angle2);

  if ((gcode_math_angle_within_arc (arc1_start_angle, arc1->sweep_angle, angle1) == 0) &&
      (gcode_math_angle_within_arc (arc2_start_angle, arc2->sweep_angle, angle2) == 0))
  {
    ip_array[*ip_num][0] = arc_ip[0];
    ip_array[*ip_num][1] = arc_ip[1];
    (*ip_num)++;
    miss = 0;
  }

  return (miss);
}

/**
 * Calculate and return the points where two primitives intersect
 * NOTE: valid points have to actually lie between each primitive's endpoints
 * NOTE: the calculations are done taking each block's offset into account
 */

int
gcode_util_intersect (gcode_block_t *block_a, gcode_block_t *block_b, gcode_vec2d_t ip_array[2], int *ip_num)
{
  if ((block_a->type == GCODE_TYPE_LINE) && (block_b->type == GCODE_TYPE_LINE))
    return line_line_intersect (block_a, block_b, ip_array, ip_num);

  if ((block_a->type == GCODE_TYPE_ARC) && (block_b->type == GCODE_TYPE_ARC))
    return arc_arc_intersect (block_a, block_b, ip_array, ip_num);

  if ((block_a->type == GCODE_TYPE_LINE) && (block_b->type == GCODE_TYPE_ARC))
    return line_arc_intersect (block_a, block_b, ip_array, ip_num);

  if ((block_a->type == GCODE_TYPE_ARC) && (block_b->type == GCODE_TYPE_LINE))
    return line_arc_intersect (block_b, block_a, ip_array, ip_num);

  return -1;
}

int
gcode_util_fillet (gcode_block_t *line1_block, gcode_block_t *line2_block, gcode_block_t *fillet_arc_block, gfloat_t radius)
{
  gcode_line_t *line1, *line2;
  gcode_arc_t *fillet_arc;
  gcode_vec2d_t vec1_u, vec2_u, vec;
  gfloat_t magnitude1, magnitude2, dot, offset, test1_angle, test2_angle;

  line1 = (gcode_line_t *)line1_block->pdata;
  line2 = (gcode_line_t *)line2_block->pdata;
  fillet_arc = (gcode_arc_t *)fillet_arc_block->pdata;

  /**
   * Assuming the end points meet at the same point then compute the slope
   * of each line and scale the end point of the current line back and push
   * the start point of the next line forward.
   * Start angle and Sweep of the fillet arc will be computed by line slopes.
   */

  /* Current Line */
  GCODE_MATH_VEC2D_SUB (vec1_u, line1->p0, line1->p1);                          /* Flipped for calculating the dot product */
  GCODE_MATH_VEC2D_MAG (magnitude1, vec1_u);
  GCODE_MATH_VEC2D_UNITIZE (vec1_u);

  /* Next Line */
  GCODE_MATH_VEC2D_SUB (vec2_u, line2->p1, line2->p0);
  GCODE_MATH_VEC2D_MAG (magnitude2, vec2_u);
  GCODE_MATH_VEC2D_UNITIZE (vec2_u);

  /**
   * To understand how this works take 2 intersecting lines that are tangent to a circle
   * of the filleting radius.  The arc midpt is along the half angle formed by the 2 lines.
   * A right triangle is formed from the arc center, tangent intersection pt, and intersection
   * of both lines.  You know one angle is 90, the other is the half angle, and the third angle
   * is known since the (3) must add to 180 degrees.  The tangent is opposite over adjacent, so using
   * the dot product to get the angle between the lines and arc tangent you can solve for the offset.
   */
  GCODE_MATH_VEC2D_DOT (dot, vec1_u, vec2_u);
  offset = radius * tan (GCODE_HPI - 0.5 * acos (dot));

  if (GCODE_MATH_IS_EQUAL (fabs (dot), 1.0))                                    // If the dot product of two unit vectors is +1.0 or -1.0, they are parallel;
    return (1);                                                                 // Therefore, somewhat hard to "fillet" in any meaningful way. So don't bother.

  /* Used by Arc */
  GCODE_MATH_VEC2D_SUB (vec1_u, line1->p1, line1->p0);
  GCODE_MATH_VEC2D_UNITIZE (vec1_u);

  /* Shorten Current Line */
  GCODE_MATH_VEC2D_SUB (vec, line1->p1, line1->p0);
  GCODE_MATH_VEC2D_SCALE (vec, (1.0 - (offset / magnitude1)));
  GCODE_MATH_VEC2D_ADD (line1->p1, vec, line1->p0);

  /* Shorten Next Line */
  GCODE_MATH_VEC2D_SUB (vec, line2->p1, line2->p0);
  GCODE_MATH_VEC2D_SCALE (vec, (1.0 - (offset / magnitude2)));
  GCODE_MATH_VEC2D_SUB (line2->p0, line2->p1, vec);

  /* Fillet Arc */
  fillet_arc->p[0] = line1->p1[0];
  fillet_arc->p[1] = line1->p1[1];
  fillet_arc->radius = radius;

  /* Sweep angle is supplemental to dot product.  dot and -dot are supplementary */
  fillet_arc->sweep_angle = GCODE_RAD2DEG * acos (-dot);

  GCODE_MATH_VEC3D_ANGLE (fillet_arc->start_angle, vec1_u[0], vec1_u[1]);
  fillet_arc->start_angle *= GCODE_RAD2DEG;

  /* Flip Test - If Line2 > 180 degrees in clockwise direction */
  GCODE_MATH_VEC3D_ANGLE (test1_angle, vec1_u[0], vec1_u[1]);
  GCODE_MATH_VEC3D_ANGLE (test2_angle, vec2_u[0], vec2_u[1]);

  if (fabs (test1_angle - test2_angle) > GCODE_PI)
  {
    if (test1_angle > test2_angle)
    {
      test2_angle += GCODE_2PI;
    }
    else
    {
      test1_angle += GCODE_2PI;
    }
  }

  if (test1_angle - test2_angle < 0.0)
  {
    fillet_arc->start_angle -= 90.0;
  }
  else
  {
    fillet_arc->start_angle += 90.0;
    fillet_arc->sweep_angle *= -1.0;
  }

  return (0);
}

/**
 * Flip the direction of a line, arc or an entire sketch (by flipping each child
 * and also flipping their order in the list - the first child becomes the last)
 */
void
gcode_util_flip_direction (gcode_block_t *block)
{
  gcode_block_t *index_block, *next_block;

  if (!block)                                                                   // If we're just going to assume 'block' exists, we'd better do it EXPLICITLY;
    return;

  switch (block->type)
  {
    case GCODE_TYPE_LINE:                                                       // Flip a single line;

      gcode_line_flip_direction (block);

      break;

    case GCODE_TYPE_ARC:                                                        // Flip a single arc;

      gcode_arc_flip_direction (block);

      break;

    case GCODE_TYPE_SKETCH:                                                     // Flip (and reverse the list of) an entire sketch;

      index_block = block->listhead;                                            // Start with the first child;

      while (index_block)                                                       // Crawl the list of 'block' one child at a time;
      {
        next_block = index_block->next;                                         // We need to remember who was next ORIGINALLY or we'll keep looping forever;

        gcode_util_flip_direction (index_block);                                // Flip the block itself;

        gcode_splice_list_around (index_block);                                 // Remove the block from the list then re-insert it as the first
        gcode_insert_as_listhead (block, index_block);                          // - this effectively flips the order of the blocks in the list;

        index_block = next_block;                                               // Continue with the block that USED TO come after 'index_block';
      }

      break;
  }
}

/**
 * Create a copy of the chain of blocks between 'start_block' and 'end_block' -
 * including both - and point 'listhead' to the first block of the copied chain;
 * NOTE: the resulting snapshot is meaningfully different from a list of clones
 * of the original - clones are meant to be valid blocks on their own that can
 * be inserted anywhere and used further freely; snapshots on the other hand are
 * meant to be used as identical work-copies of the originals and as such they
 * keep their original's 'name' field, 'pretending' to be the original block;
 * NOTE: 'start_block' and 'end_block' should belong to the same list, in this
 * order; if they don't, simply everything after 'start_block' is returned;
 * NOTE: while strictly speaking copying an entire list would mean passing the
 * first and last block of the list as parameters, the implementation specifics
 * make it possible to just pass 'NULL' as the second block for the same effect;
 */

int
gcode_util_get_sublist_snapshot (gcode_block_t **listhead, gcode_block_t *start_block, gcode_block_t *end_block)
{
  gcode_block_t *index_block, *new_block, *last_block;

  *listhead = NULL;

  if (!start_block)
    return (1);

  index_block = start_block;

  while (index_block)
  {
    index_block->clone (&new_block, index_block->gcode, index_block);

    new_block->name = index_block->name;

    if (*listhead)
    {
      gcode_insert_after_block (last_block, new_block);
    }
    else
    {
      *listhead = new_block;
    }

    if (index_block == end_block)
      break;

    last_block = new_block;
    index_block = index_block->next;
  }

  return (0);
}

int
gcode_util_remove_null_sections (gcode_block_t **listhead)
{
  gcode_block_t *index_block, *next_block;

  if (!(*listhead))
    return (1);

  index_block = *listhead;

  while (index_block)
  {
    switch (index_block->type)
    {
      case GCODE_TYPE_LINE:
      {
        gcode_line_t *line;

        line = (gcode_line_t *)index_block->pdata;

        if (GCODE_MATH_2D_MANHATTAN (line->p0, line->p1) >= GCODE_PRECISION)
        {
          index_block = index_block->next;
          continue;
        }

        break;
      }

      case GCODE_TYPE_ARC:
      {
        gcode_arc_t *arc;

        arc = (gcode_arc_t *)index_block->pdata;

        if (arc->radius >= GCODE_PRECISION)
        {
          index_block = index_block->next;
          continue;
        }

        break;
      }
    }

    next_block = index_block->next;

    if (index_block->next)
      index_block->next->prev = index_block->prev;

    if (index_block->prev)
      index_block->prev->next = index_block->next;

    if (*listhead == index_block)
      *listhead = next_block;

    index_block->free (&index_block);

    index_block = next_block;
  }

  return (0);
}

/**
 * Rearrange and/or flip the blocks in the list that starts with 'listhead' in a
 * way that results in the longest contiguous fragments possible, then return 1
 * if the resulting contour is a closed one or 0 if it is not. If open fragments
 * are unavoidable then 'listhead' will point to one of the ends of one of them,
 * otherwise an attempt is made to retain the original starting point as well as
 * the original direction the majority of the blocks in the list are facing in;
 * NOTE: this will return 1 even if multiple unconnected fragments are found, as
 * long as each one is closed;
 * NOTE: although practical observed performance (speed) of this function seems 
 * quite adequate in ordinary conditions, it could slow down significantly for
 * inconveniently ordered large lists; the best bet to avoid that is keeping 
 * lists correctly ordered in the first place thereby reducing the amount of 
 * processing involved from quadratic to linear in relation to list size;
 */

int
gcode_util_merge_list_fragments (gcode_block_t **listhead)
{
  gcode_block_t *prev_edge_block, *next_edge_block, *index_block;
  gcode_vec2d_t e, pe, ne, e0, e1;
  int closed, block_count, flip_count, break_count;

  if (!(*listhead))                                                             // Nothing to sort at all...? Oh great - we're done!
    return (1);

  if (!(*listhead)->next)                                                       // A single, lonely block...? Well... see if it's closed, THEN we're done.
  {
    (*listhead)->ends (*listhead, e0, e1, GCODE_GET);

    if (GCODE_MATH_2D_DISTANCE (e0, e1) < GCODE_TOLERANCE)
      return (1);
    else
      return (0);
  }

  closed = 1;                                                                   // Okay, crunch time; the list is 'closed' until proven guil... erm, 'open'.

  block_count = 1;                                                              // Block count starts from 1 because the loop runs 'n-1' times, not 'n';
  flip_count = 0;                                                               // The rest of the counters start from zero as all well-behaved counters do.
  break_count = 0;

  prev_edge_block = *listhead;                                                  // These two shall keep track of the edges of the last contiguous fragment;
  next_edge_block = *listhead;

  while (next_edge_block->next)                                                 // As long there are blocks PAST the 'next' edge, keep looping (hence 'n-1');
  {
    prev_edge_block->ends (prev_edge_block, pe, e, GCODE_GET);                  // Get hold of the edge endpoints of the current fragment, as 'pe' and 'ne';
    next_edge_block->ends (next_edge_block, e, ne, GCODE_GET);

    index_block = next_edge_block->next;                                        // Start looking for match candidates starting right after the sorted edge;

    while (index_block)                                                         // Keep looping (and looking for matches) until the list ends;
    {
      index_block->ends (index_block, e0, e1, GCODE_GET);                       // Obtain the two endpoints of the current block, as 'e0' and 'e1';

      if (GCODE_MATH_2D_DISTANCE (e0, ne) < GCODE_TOLERANCE)                    // If the current block fits right after the current 'next' edge...
      {
        if (next_edge_block->next != index_block)                               // ...but it's not actually the block next to it,
          gcode_place_block_behind (next_edge_block, index_block);              // MAKE IT be the block next to the edge (slide it within the list);

        next_edge_block = index_block;                                          // Once that's done, this block becomes the new edge;

        break;                                                                  // Since a match was found, abandon the search and move on to the next block;
      }

      if (GCODE_MATH_2D_DISTANCE (e1, ne) < GCODE_TOLERANCE)                    // If the current block would fit after the current 'next' edge IF FLIPPED...
      {
        flip_count++;                                                           // ...do just that: flip it - but count each time a block gets flipped;

        gcode_util_flip_direction (index_block);

        if (next_edge_block->next != index_block)                               // Anyway, if the block is not actually next to the edge,
          gcode_place_block_behind (next_edge_block, index_block);              // MAKE IT be the block next to the edge (slide it within the list);

        next_edge_block = index_block;                                          // Once that's done, this block becomes the new edge;

        break;                                                                  // Since a match was found, abandon the search and move on to the next block;
      }

      if (GCODE_MATH_2D_DISTANCE (e1, pe) < GCODE_TOLERANCE)                    // If the current block fits right before the current 'prev' edge...
      {
        if (prev_edge_block->prev != index_block)                               // ...but it's not actually the block next to it,
          gcode_place_block_before (prev_edge_block, index_block);              // MAKE IT be the block next to the edge (slide it within the list);

        prev_edge_block = index_block;                                          // Once that's done, this block becomes the new edge;

        break;                                                                  // Since a match was found, abandon the search and move on to the next block;
      }

      if (GCODE_MATH_2D_DISTANCE (e0, pe) < GCODE_TOLERANCE)                    // If the current block would fit before the current 'prev' edge IF FLIPPED...
      {
        flip_count++;                                                           // ...do just that: flip it - but count each time a block gets flipped;

        gcode_util_flip_direction (index_block);

        if (prev_edge_block->prev != index_block)                               // Anyway, if the block is not actually next to the edge,
          gcode_place_block_before (prev_edge_block, index_block);              // MAKE IT be the block next to the edge (slide it within the list);

        prev_edge_block = index_block;                                          // Once that's done, this block becomes the new edge;

        break;
      }

      index_block = index_block->next;                                          // If there's no way the current block fits anywhere, try the next one;
    }

    if (!index_block)                                                           // If 'index_block' got to become NULL, we ran out of blocks without a match;
    {
      break_count++;                                                            // That means whatever is left is part of one or more unconnected fragments;

      if (GCODE_MATH_2D_DISTANCE (ne, pe) > GCODE_TOLERANCE)                    // We still want to know whether the CURRENT fragment is closed, though:
        closed = 0;                                                             // if the endpoints of its edges aren't the same, then it clearly isn't.

      next_edge_block = next_edge_block->next;                                  // Either way, start a new fragment by appointing the first block
      prev_edge_block = next_edge_block;                                        // past the 'next' edge as both the new 'prev' and 'next' edge;
    }

    block_count++;                                                              // And while we're at it, remember to count the number of blocks in the list;
  }

  /* The list is now sorted into as few fragments as possible */

  prev_edge_block->ends (prev_edge_block, pe, e, GCODE_GET);                    // The sorting is done, but the last fragment was not yet checked for closure;
  next_edge_block->ends (next_edge_block, e, ne, GCODE_GET);                    // The endpoints are potentially outdated by now - we have to get them again;

  if (GCODE_MATH_2D_DISTANCE (ne, pe) > GCODE_TOLERANCE)                        // If they are apart, the fragment isn't closed (hence neither is the list);
    closed = 0;

  while (prev_edge_block->prev)                                                 // But the current 'edges' only delimit the last fragment; to find the actual
    prev_edge_block = prev_edge_block->prev;                                    // edges of the list, we have to expand them along the list as far as they go;

  while (next_edge_block->next)
    next_edge_block = next_edge_block->next;

  prev_edge_block->ends (prev_edge_block, pe, e, GCODE_GET);                    // Now that we found the edges of the entire list, get the THOSE endpoints;
  next_edge_block->ends (next_edge_block, e, ne, GCODE_GET);                    // The list may well be sorted, but the listhead might no longer be valid.

  /* Convenience feature: try to keep the original listhead if possible */

  if (GCODE_MATH_2D_DISTANCE (ne, pe) < GCODE_TOLERANCE)                        // So we could just appoint the 'prev' edge as the new head of the list, but...
  {
    prev_edge_block->prev = next_edge_block;                                    // ...if the list is closed, the old listhead is just as good as any new one;
    next_edge_block->next = prev_edge_block;                                    // So we try keeping it by first connecting the current list edges together...

    (*listhead)->prev->next = NULL;                                             // ...then sectioning the now-circular list right before the old listhead;
    (*listhead)->prev = NULL;
  }
  else                                                                          // Yeah, or we could just appoint the 'prev' edge as the new head of the list.
  {
    *listhead = prev_edge_block;
  }

  /* Convenience feature: try to keep the direction of the majority of blocks */

  if (flip_count > block_count / 2)                                             // If more than half of all blocks have been flipped, flip all blocks again;
  {
    prev_edge_block = *listhead;                                                // This will keep track of the current 'first' at all times;

    gcode_util_flip_direction (prev_edge_block);                                // Flip the first block;

    index_block = prev_edge_block->next;                                        // Start with the SECOND block - we cannot move the first one before itself;

    while (index_block)                                                         // Crawl along the list one block at a time;
    {
      next_edge_block = index_block->next;                                      // We need to remember who was next ORIGINALLY or we'll keep looping forever;

      gcode_util_flip_direction (index_block);                                  // Flip the block itself;

      gcode_place_block_before (prev_edge_block, index_block);                  // Remove the block from the list then re-insert it before the first one;

      prev_edge_block = index_block;                                            // The moved block is now therefore effectively the new first one;
      index_block = next_edge_block;                                            // Continue with the block that USED TO come after 'index_block';
    }

    /* Convenience feature: try to keep the listhead even after a full flip */

    if ((break_count == 0) && closed)                                           // Since the list is flipped, we need to update the listhead:
      gcode_place_block_before (prev_edge_block, *listhead);                    // either bring it back from the tail to the head of the list
    else                                                                        // (if the list is closed and 'rolling it over' is possible),
      *listhead = prev_edge_block;                                              // or else we change the listhead to point to the new head...
  }

  return (closed);
}

/**
 * Take each block in the list starting with 'listhead' and apply to it whatever 
 * offset it is linked to; in other words, calculate how the linked offset would
 * change each primitive and update each with the new data, then re-link it to
 * the same (newly created) zero-offset record that should eventually be freed 
 * when the list itself is disposed of;
 */

int
gcode_util_convert_to_no_offset (gcode_block_t *listhead)
{
  gcode_offset_t *zero_offset;
  gcode_block_t *index_block;

  if (!listhead)
    return (1);

  zero_offset = (gcode_offset_t *) malloc (sizeof (gcode_offset_t));
  zero_offset->side = listhead->offset->side;
  zero_offset->tool = 0.0;
  zero_offset->eval = 0.0;
  zero_offset->origin[0] = 0.0;
  zero_offset->origin[1] = 0.0;
  zero_offset->rotation = 0.0;

  index_block = listhead;

  while (index_block)
  {
    switch (index_block->type)
    {
      case GCODE_TYPE_LINE:
      {
        gcode_line_t *line;
        gcode_vec2d_t p0, p1, normal;

        line = (gcode_line_t *)index_block->pdata;
        gcode_line_with_offset (index_block, p0, p1, normal);

        line->p0[0] = p0[0];
        line->p0[1] = p0[1];

        line->p1[0] = p1[0];
        line->p1[1] = p1[1];

        break;
      }

      case GCODE_TYPE_ARC:
      {
        gcode_arc_t *arc;
        gcode_vec2d_t p0, p1, center;
        gfloat_t radius, start_angle;

        arc = (gcode_arc_t *)index_block->pdata;
        gcode_arc_with_offset (index_block, p0, center, p1, &radius, &start_angle);

        arc->radius = radius;
        arc->p[0] = p0[0];
        arc->p[1] = p0[1];
        arc->start_angle = start_angle;

        break;
      }
    }

    index_block->offset = zero_offset;
    index_block = index_block->next;
  }

  return (0);
}
