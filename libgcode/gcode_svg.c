/**
 *  gcode_svg.c
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

#include "gcode_svg.h"
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <libgen.h>
#include <expat.h>

/**
 * Convert a center-defined elliptic arc into an endpoint-defined one.
 * NOTE: Angles 'phi', 'theta' and 'sweep' must be / are in RADIANS!!!
 * Implements the algorithm from 'SVG Implementation Notes' at W3.org;
 */

static gcode_arc_by_points_t
gcode_arc_center_to_points (gcode_arc_by_center_t arc1)
{
  double x1, y1, x2, y2;
  double rx, ry, phi;
  int fla, fls;

  double cx, cy;
  double theta, sweep;

  gcode_arc_by_points_t arc2;

  cx = arc1.cpt[0];
  cy = arc1.cpt[1];
  rx = arc1.rx;
  ry = arc1.ry;
  phi = arc1.phi;
  theta = arc1.theta;
  sweep = arc1.sweep;

  x1 = rx * cos (phi) * cos (theta) - ry * sin (phi) * sin (theta) + cx;
  y1 = rx * sin (phi) * cos (theta) + ry * cos (phi) * sin (theta) + cy;

  x2 = rx * cos (phi) * cos (theta + sweep) - ry * sin (phi) * sin (theta + sweep) + cx;
  y2 = rx * sin (phi) * cos (theta + sweep) + ry * cos (phi) * sin (theta + sweep) + cy;

  fla = (fabs (sweep) > GCODE_PI) ? 1 : 0;

  fls = (sweep > 0) ? 1 : 0;

  arc2.pt0[0] = x1;
  arc2.pt0[1] = y1;
  arc2.pt1[0] = x2;
  arc2.pt1[1] = y2;
  arc2.rx = rx;
  arc2.ry = ry;
  arc2.phi = phi;
  arc2.fla = fla;
  arc2.fls = fls;

  return (arc2);
}

/**
 * Convert an endpoint-defined elliptic arc into a center-defined one.
 * NOTE: Angles 'phi', 'theta' and 'sweep' must be / are in RADIANS!!!
 * Implements the algorithm from 'SVG Implementation Notes' at W3.org;
 */

static gcode_arc_by_center_t
gcode_arc_points_to_center (gcode_arc_by_points_t arc1)
{
  double x1, y1, x2, y2;
  double rx, ry, phi;
  int fla, fls;

  double xp, yp;
  double cxp, cyp;
  double cx, cy;
  double ux, uy;
  double vx, vy;
  double lambda;
  double factor;
  double theta, sweep;

  gcode_arc_by_center_t arc2;

  x1 = arc1.pt0[0];
  y1 = arc1.pt0[1];
  x2 = arc1.pt1[0];
  y2 = arc1.pt1[1];
  rx = arc1.rx;
  ry = arc1.ry;
  phi = arc1.phi;
  fla = arc1.fla;
  fls = arc1.fls;

  /* SVG Implementation Notes - F.6.2 */

  fla = fla ? 1 : 0;
  fls = fls ? 1 : 0;

  /* SVG Implementation Notes - F.6.6 - Step 2 */

  rx = fabs (rx);
  ry = fabs (ry);

  /* SVG Implementation Notes - F.6.5 - Step 1 */

  xp = ((x1 - x2) / 2) * cos (phi) + ((y1 - y2) / 2) * sin (phi);
  yp = ((y1 - y2) / 2) * cos (phi) - ((x1 - x2) / 2) * sin (phi);

  /* SVG Implementation Notes - F.6.6 - Step 3 */

  lambda = (xp * xp) / (rx * rx) + (yp * yp) / (ry * ry);

  if (lambda > 1)
  {
    rx *= sqrt (lambda);
    ry *= sqrt (lambda);
  }

  /* SVG Implementation Notes - F.6.5 - Step 2 */

  factor = sqrt (((rx * rx * ry * ry) - (rx * rx * yp * yp) - (ry * ry * xp * xp)) / ((rx * rx * yp * yp) + (ry * ry * xp * xp)));

  factor = (fla == fls) ? 0 - factor : factor;                                  // Okay, so I feel better expressing (-x) as (0 - x); SO WHAT?

  cxp = 0 + factor * yp * rx / ry;
  cyp = 0 - factor * xp * ry / rx;

  /* SVG Implementation Notes - F.6.5 - Step 3 */

  cx = cxp * cos (phi) - cyp * sin (phi) + (x1 + x2) / 2;
  cy = cxp * sin (phi) + cyp * cos (phi) + (y1 + y2) / 2;

  /* SVG Implementation Notes - F.6.5 - Step 4 */

  ux = 1;
  uy = 0;

  vx = (xp - cxp) / rx;
  vy = (yp - cyp) / ry;

  theta = acos ((ux * vx + uy * vy) / (sqrt (ux * ux + uy * uy) * sqrt (vx * vx + vy * vy)));

  theta = ((ux * vy - uy * vx) < 0) ? 0 - theta : theta;

  ux = vx;
  uy = vy;

  vx = (0 - xp - cxp) / rx;
  vy = (0 - yp - cyp) / ry;

  sweep = acos ((ux * vx + uy * vy) / (sqrt (ux * ux + uy * uy) * sqrt (vx * vx + vy * vy)));

  sweep = ((ux * vy - uy * vx) < 0) ? 0 - sweep : sweep;

  if (fls == 0)
  {
    if (sweep > 0)
      sweep -= GCODE_2PI;
  }
  else
  {
    if (sweep < 0)
      sweep += GCODE_2PI;
  }

  arc2.cpt[0] = cx;
  arc2.cpt[1] = cy;
  arc2.rx = rx;
  arc2.ry = ry;
  arc2.phi = phi;
  arc2.theta = theta;
  arc2.sweep = sweep;

  return (arc2);
}

/**
 * Pure maths routine computing a point along a quadratic Bézier curve starting
 * at 'p0', ending at 'p2' and controlled by 'p1' while 't' goes from 0 to 1;
 * also, while 'points' are being mentioned, both the arguments and the result 
 * are mono-dimensional - for a 2D point, two calls are required;
 */

static double
gcode_svg_quadratic_bezier (double t, double p0, double p1, double p2)
{
  return (pow ((1 - t), 2) * p0 + 2 * (1 - t) * t * p1 + pow (t, 2) * p2);
}

/**
 * Pure maths routine computing a point along a cubic Bézier curve starting at
 * 'p0', ending at 'p3' and controlled by 'p1' and 'p2' while 't' goes from 0
 * to 1; also, while 'points' are being mentioned, both the arguments and the 
 * result are mono-dimensional - for a 2D point, two calls are required;
 */

static double
gcode_svg_cubic_bezier (double t, double p0, double p1, double p2, double p3)
{
  return (pow ((1 - t), 3) * p0 + 3 * pow ((1 - t), 2) * t * p1 + 3 * (1 - t) * pow (t, 2) * p2 + pow (t, 3) * p3);
}

/**
 * Create a new line block representing a line from pt0 to pt1 and then append 
 * it under the current sketch block retrieved from the SVG context;
 * NOTE: The coordinates of pt0 and pt1 are in the SVG reference frame, they are
 * neither scaled to GCAM units nor translated from the SVG 'top-left' frame to 
 * the GCAM 'bottom-left' one; these are values as taken directly from the file.
 */

static int
gcode_svg_create_line (void *context, double pt0[], double pt1[])
{
  gcode_block_t *line_block;
  gcode_line_t *line;

  gcode_svg_t *svg = (gcode_svg_t *) context;                                   // First things first: retrieve a reference to the SVG context;

  if (!svg->sketch_block)                                                       // If the parent is missing, what should we attach to?
    return (0);                                                                 // Report "no items appended" and leave;

  gcode_line_init (&line_block, svg->gcode, NULL);                              // Create a new line block;

  line = (gcode_line_t *)line_block->pdata;                                     // Get a reference to its line-specific data structure;

  line->p0[0] = (gfloat_t)pt0[0] * svg->scale[0];                               // The incoming numbers are as-read (unscaled) - they need to be scaled;
  line->p0[1] = svg->size[1] - (gfloat_t)pt0[1] * svg->scale[1];                // Also, SVG (0,0) is top-left - convert to our bottom-left system;
  line->p1[0] = (gfloat_t)pt1[0] * svg->scale[0];
  line->p1[1] = svg->size[1] - (gfloat_t)pt1[1] * svg->scale[1];

  gcode_append_as_listtail (svg->sketch_block, line_block);                     // Append the new line block under the current sketch block.

  return (1);                                                                   // Report "one item appended".
}

/**
 * Create a new arc block representing an arc starting in pt with radius r and 
 * then append it under the current sketch block retrieved from the SVG context;
 * NOTE: The coordinates of point 'pt' are in the SVG reference frame, they are
 * neither scaled to GCAM units nor translated from the SVG 'top-left' frame to 
 * the GCAM 'bottom-left' one; these are values as taken directly from the file.
 */

static int
gcode_svg_create_arc (void *context, double pt[], double r, double start, double sweep)
{
  gcode_block_t *arc_block;
  gcode_arc_t *arc;

  gcode_svg_t *svg = (gcode_svg_t *) context;                                   // First things first: retrieve a reference to the SVG context;

  if (!svg->sketch_block)                                                       // If the parent is missing, what should we attach to?
    return (0);                                                                 // Report "no items appended" and leave;

  gcode_arc_init (&arc_block, svg->gcode, NULL);                                // Create a new arc block;

  arc = (gcode_arc_t *)arc_block->pdata;                                        // Get a reference to its arc-specific data structure;

  arc->p[0] = (gfloat_t)pt[0] * svg->scale[0];                                  // The incoming numbers are as-read (unscaled) - they need to be scaled;
  arc->p[1] = svg->size[1] - (gfloat_t)pt[1] * svg->scale[1];                   // Also, SVG (0,0) is top-left - convert to our bottom-left system;
  arc->radius = (gfloat_t)r * ((svg->scale[0] + svg->scale[1]) / 2);            // This will fail big-time if the scales differ - so check for that;
  arc->start_angle = (gfloat_t)start;
  GCODE_MATH_WRAP_TO_360_DEGREES (arc->start_angle);                            // A negative start angle is not valid in GCAM - make sure it isn't;
  GCODE_MATH_SNAP_TO_360_DEGREES (arc->start_angle);                            // Try to prevent rounding fuzz and fmod() fuck-ups (360.0 returned);
  arc->sweep_angle = (gfloat_t)sweep;                                           // Sweeps are valid between +/-360 degrees, no need to correct this;
  gcode_append_as_listtail (svg->sketch_block, arc_block);                      // Append the new arc block under the current sketch block.

  return (1);
}

/**
 * Create new line or arc blocks representing an elliptic arc described by its
 * endpoints then append them under the current SVG sketch using 'create_xxxx';
 * NOTE: The coordinates of pt0, pt1 are in the SVG reference frame, they are
 * neither scaled to GCAM units nor translated from the SVG 'top-left' frame to 
 * the GCAM 'bottom-left' one; these are values as taken directly from the file.
 * The angle 'phi' is expected to be in degrees (-360 ... 360).
 */

static int
gcode_svg_create_elliptic_arc (void *context, double pt0[], double pt1[], double rx, double ry, double phi, int fla, int fls)
{
  double t;
  int segments;
  int n, items = 0;

  gcode_arc_by_points_t arc1;
  gcode_arc_by_center_t arc2;

  gcode_svg_t *svg = (gcode_svg_t *) context;                                   // First things first: retrieve a reference to the SVG context;

  segments = svg->gcode->curve_segments;                                        // Retrieve the number of segments used to approximate a curve;

  if ((pt0[0] == pt1[0]) && (pt0[1] == pt1[1]))                                 // Standard SVG bailout (SVG Implementation Notes - F.6.2);
  {
    return (0);
  }
  else if ((rx == 0) || (ry == 0))                                              // Standard SVG bailout (SVG Implementation Notes - F.6.6 - Step 1);
  {
    items += gcode_svg_create_line (context, pt0, pt1);                         // If either radius is zero, just draw a line, full stop;
  }
  else                                                                          // It seems we're not getting off the hook that easily;
  {
    phi = fmod (phi, 360.0) * GCODE_DEG2RAD;                                    // Convert 'phi' from degrees to radians;

    arc1.pt0[0] = pt0[0];                                                       // Haul all the data over into a convenient points-based struct;
    arc1.pt0[1] = pt0[1];
    arc1.pt1[0] = pt1[0];
    arc1.pt1[1] = pt1[1];
    arc1.rx = rx;
    arc1.ry = ry;
    arc1.phi = phi;
    arc1.fla = fla;
    arc1.fls = fls;

    arc2 = gcode_arc_points_to_center (arc1);                                   // Handwave the data into another convenient center-based struct;

    if ((arc2.rx == arc2.ry) && (svg->scale[0] == svg->scale[1]))               // If the "ellipse" is in fact a circle and the X/Y scales are equal(!),
    {                                                                           // then it can be realized with a circular arc instead of line segments;
      double radius, start, sweep;

      radius = arc2.rx;

      start = 0 - arc2.theta * GCODE_RAD2DEG;                                   // The SVG "Y" axis being mirrored vs. the GCAM "Y",
      sweep = 0 - arc2.sweep * GCODE_RAD2DEG;                                   // angles get "mirrored" (and converted to degrees);

      items += gcode_svg_create_arc (context, pt0, radius, start, sweep);
    }
    else                                                                        // If the arc is indeed elliptic, we must approximate it with lines;
    {
      arc2.sweep /= segments;                                                   // Reduce the sweep of the conversion result to a fraction of itself;

      for (n = 1; n <= segments; n++)                                           // Create a bunch of consecutive sub-arcs with that fractional sweep;
      {
        t = (double)n / segments;                                               // Funnily enough, looping directly on 't' would have issues comparing to "1";

        arc1 = gcode_arc_center_to_points (arc2);                               // Find out where the endpoints of the current sub-arc would be;

        items += gcode_svg_create_line (context, arc1.pt0, arc1.pt1);           // Approximate it with a straight line between those endpoints;

        arc2.theta += arc2.sweep;                                               // Move on to the next sub-arc by adding the sweep fraction to the start angle.
      }
    }
  }

  return (items);
}

/**
 * Create new line blocks representing a quadratic Bézier curve described by pt0
 * ... pt2 then append them under the current SVG sketch using 'create_line';
 * NOTE: The coordinates of pt0 to pt2 are in the SVG reference frame, they are
 * neither scaled to GCAM units nor translated from the SVG 'top-left' frame to 
 * the GCAM 'bottom-left' one; these are values as taken directly from the file.
 */

static int
gcode_svg_create_quadratic_bezier (void *context, double pt0[], double pt1[], double pt2[])
{
  double t;
  double ptA[2], ptB[2];

  int segments;
  int n, items = 0;

  gcode_svg_t *svg = (gcode_svg_t *) context;                                   // First things first: retrieve a reference to the SVG context;

  segments = svg->gcode->curve_segments;                                        // Retrieve the number of segments used to approximate a curve;

  ptA[0] = pt0[0];
  ptA[1] = pt0[1];

  for (n = 1; n <= segments; n++)
  {
    t = (double)n / segments;

    ptB[0] = gcode_svg_quadratic_bezier (t, pt0[0], pt1[0], pt2[0]);
    ptB[1] = gcode_svg_quadratic_bezier (t, pt0[1], pt1[1], pt2[1]);

    items += gcode_svg_create_line (context, ptA, ptB);

    ptA[0] = ptB[0];
    ptA[1] = ptB[1];
  }

  return (items);
}

/**
 * Create new line blocks representing a cubic Bézier curve described by pt0 ...
 * pt3 then append them under the current SVG sketch using 'create_line';
 * NOTE: The coordinates of pt0 to pt3 are in the SVG reference frame, they are
 * neither scaled to GCAM units nor translated from the SVG 'top-left' frame to 
 * the GCAM 'bottom-left' one; these are values as taken directly from the file.
 */

static int
gcode_svg_create_cubic_bezier (void *context, double pt0[], double pt1[], double pt2[], double pt3[])
{
  double t;
  double ptA[2], ptB[2];

  int segments;
  int n, items = 0;

  gcode_svg_t *svg = (gcode_svg_t *) context;                                   // First things first: retrieve a reference to the SVG context;

  segments = svg->gcode->curve_segments;                                        // Retrieve the number of segments used to approximate a curve;

  ptA[0] = pt0[0];
  ptA[1] = pt0[1];

  for (n = 1; n <= segments; n++)
  {
    t = (double)n / segments;

    ptB[0] = gcode_svg_cubic_bezier (t, pt0[0], pt1[0], pt2[0], pt3[0]);
    ptB[1] = gcode_svg_cubic_bezier (t, pt0[1], pt1[1], pt2[1], pt3[1]);

    items += gcode_svg_create_line (context, ptA, ptB);

    ptA[0] = ptB[0];
    ptA[1] = ptB[1];
  }

  return (items);
}

/**
 * Thanks to neither atof() nor sscanf() being willing to disclose WHERE EXACTLY
 * they stopped parsing the string they work on, we need some other way to tell
 * where to continue looking for numbers in a chunk after the first set has been
 * parsed - well, this is it. Basically this takes a pointer into a string and 
 * moves it forward by the number of "numbers" specified by 'amount', hopefully
 * skipping the exact same part that sscanf() just consumed and converted.
 * NOTE: THIS IS AN IMPERFECT AND NAIVE IMPLEMENTATION, yadda yadda, yes I KNOW.
 * This thing only works as long as numbers are clearly separated by non-numeric
 * characters, while in SVG "0.5.5" is perfectly valid for "0.5, 0.5" (thanks a
 * lot for all the optionally valid stuff in there by the way) - well it's tough
 * luck, mate; any SVG produced with some half-decent effort should work fine,
 * you'll just have to pass any ultra-condensed markup through a beautifier...
 */

static void
gcode_svg_seek_ahead (char **index, int amount)
{
  if (amount == 0)                                                              // This is a special case: 0 means "seek to the \0" (all the way to the end);
  {
    *index += strlen (*index);                                                  // Advance 'index' with the full length of the string, landing it on the '\0';
  }
  else                                                                          // Otherwise, start detecting numbers and skip a number of 'amount' of them;
  {
    while (**index && amount)                                                   // Keep skipping as long as the string did not end, and 'amount' isn't zero;
    {
      *index += strcspn (*index, SVG_NUMERIC_CHARS);                            // First, skip everything that ISN'T part of a number;
      *index += strspn (*index, SVG_NUMERIC_CHARS);                             // Then, skip everything that IS part of a number - easy, huh?

      amount--;                                                                 // One number skipped (fingers crossed...);
    }
  }
}

/**
 * Parse the string 'path' for SVG path commands, create 'line' and 'arc' blocks
 * corresponding to the encountered path elements and add them to a new sketch
 * block, then append that sketch under the parent block from the SVG context;
 * NOTE: While efforts have been made to reasonably get this parser to work with
 * different styles of markup, IT IS BY NO MEANS some ISO-standard compliant and
 * feature-complete reference implementation; if you try to drive it into a wall
 * on purpose, you'll definitely succeed without much effort. It's barebone, OK?
 */

static void
gcode_svg_parse_path_data (void *context, char *path)
{
  char *buffer, *chunk, *index;
  int length, items;
  int fla, fls;
  char history;
  double value;
  double rx, ry, phi;
  double pen[2] = { 0.0 };
  double start[2] = { 0.0 };
  double pt1[2], pt2[2], pt3[2], ptc[2];

  gcode_svg_t *svg = (gcode_svg_t *) context;                                   // First things first: retrieve a reference to the SVG context;

  items = 0;                                                                    // Reset the number if items imported;
  buffer = NULL;                                                                // Prepare the buffer pointer for fresh realloc();
  history = 'Z';                                                                // A new path is the same thing as the last command being "close path" ('Z');

  gcode_sketch_init (&svg->sketch_block, svg->gcode, NULL);                     // Create a new sketch block to import things into;

  chunk = strpbrk (path, SVG_PATH_COMMANDS);                                    // Find the sub-string of the path that begins with a command letter;

  if (!chunk)                                                                   // If none are found at all, free the new sketch and leave;
  {
    svg->sketch_block->free (&svg->sketch_block);
    return;
  }

  while (*chunk)                                                                // If the remaining chunk of the path string is not an empty string,
  {
    length = strcspn (chunk + 1, SVG_PATH_COMMANDS) + 1;                        // find out how much of it is free of further command letters;

    buffer = realloc (buffer, length + 1);                                      // That part will become the next chunk, so allocate some space for it;

    if (!buffer)                                                                // If there was a problem allocating the buffer, free the sketch and leave;
    {
      svg->sketch_block->free (&svg->sketch_block);
      return;
    }

    strncpy (buffer, chunk, length);                                            // Copy the current chunk of path dataover into the buffer,
    buffer[length] = '\0';                                                      // and be sure to terminate it properly.

    for (index = buffer; *index; index++)                                       // The only SVG 'separator' sscanf can't deal with is ',' - get rid of it...
      if (*index == ',')
        *index = ' ';

    index = buffer + 1;                                                         // Start parsing the current chunk from right after the command letter;

    do                                                                          // Since multiple value sets for a single command are valid, we have to loop.
    {
      switch (buffer[0])                                                        // Examine the command and let the boredom commence - this list never ends...
      {
        case 'M':                                                               // >>> 'Absolute move':

          if (SVG_PARSE_1X_POINT (pt1, index))                                  // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 2);                                   // move the chunk parser pointer a corresponding amount ahead;

            start[0] = pt1[0];                                                  // Allegedly, this is ALWAYS a new path - store the position as the new start;
            start[1] = pt1[1];                                                  // worst case (if pen was not set this round either): it stays zero;

            pen[0] = pt1[0];                                                    // Update the current position to the new value;
            pen[1] = pt1[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'm':                                                               // >>> 'Relative move':

          if (SVG_PARSE_1X_POINT (pt1, index))                                  // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 2);                                   // move the chunk parser pointer a corresponding amount ahead;

            start[0] = pt1[0];                                                  // See above - store the new position as the new start;
            start[1] = pt1[1];                                                  // worst case (if pen was not set this round either): it stays zero;

            pen[0] += pt1[0];                                                   // NOTE: While a first-in-path relative 'moveto' is to be considered absolute,
            pen[1] += pt1[1];                                                   // 'pen' is inited to 0, and an absolute or a relative-to-0 move are the same!
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'L':                                                               // >>> 'Absolute line':

          if (SVG_PARSE_1X_POINT (pt1, index))                                  // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 2);                                   // move the chunk parser pointer a corresponding amount ahead;

            items += gcode_svg_create_line (context, pen, pt1);                 // Create the actual object out of lines and/or arcs, record the number added;

            pen[0] = pt1[0];                                                    // Update the current position to the new value;
            pen[1] = pt1[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'l':                                                               // >>> 'Relative line':

          if (SVG_PARSE_1X_POINT (pt1, index))                                  // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 2);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] += pen[0];                                                   // Transform the relative values into absolute ones;
            pt1[1] += pen[1];

            items += gcode_svg_create_line (context, pen, pt1);                 // Create the actual object out of lines and/or arcs, record the number added;

            pen[0] = pt1[0];                                                    // Update the current position to the new value;
            pen[1] = pt1[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'H':                                                               // >>> 'Absolute horizontal line':

          if (SVG_PARSE_1X_VALUE (value, index))                                // If a value could be read,
          {
            gcode_svg_seek_ahead (&index, 1);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] = value;                                                     // Fill out a 2D point with the new values;
            pt1[1] = pen[1];

            items += gcode_svg_create_line (context, pen, pt1);                 // Create the actual object out of lines and/or arcs, record the number added;

            pen[0] = pt1[0];                                                    // Update the current position to the new value;
          }
          else                                                                  // If a value could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'h':                                                               // >>> 'Relative horizontal line':

          if (SVG_PARSE_1X_VALUE (value, index))                                // If a value could be read,
          {
            gcode_svg_seek_ahead (&index, 1);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] = pen[0] + value;                                            // Transform the relative value into an absolute one and fill out a 2D point;
            pt1[1] = pen[1];

            items += gcode_svg_create_line (context, pen, pt1);                 // Create the actual object out of lines and/or arcs, record the number added;

            pen[0] = pt1[0];                                                    // Update the current position to the new value;
          }
          else                                                                  // If a value could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'V':                                                               // >>> 'Absolute vertical line':

          if (SVG_PARSE_1X_VALUE (value, index))                                // If a value could be read,
          {
            gcode_svg_seek_ahead (&index, 1);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] = pen[0];                                                    // Fill out a 2D point with the new values;
            pt1[1] = value;

            items += gcode_svg_create_line (context, pen, pt1);                 // Create the actual object out of lines and/or arcs, record the number added;

            pen[1] = pt1[1];                                                    // Update the current position to the new value;
          }
          else                                                                  // If a value could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'v':                                                               // >>> 'Relative vertical line':

          if (SVG_PARSE_1X_VALUE (value, index))                                // If a value could be read,
          {
            gcode_svg_seek_ahead (&index, 1);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] = pen[0];                                                    // Transform the relative value into an absolute one and fill out a 2D point;
            pt1[1] = pen[1] + value;

            items += gcode_svg_create_line (context, pen, pt1);                 // Create the actual object out of lines and/or arcs, record the number added;

            pen[1] = pt1[1];                                                    // Update the current position to the new value;
          }
          else                                                                  // If a value could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'A':                                                               // >>> 'Absolute elliptical arc':

          if (SVG_PARSE_ARC_DATA (rx, ry, phi, fla, fls, pt1, index))           // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 7);                                   // move the chunk parser pointer a corresponding amount ahead;

            items += gcode_svg_create_elliptic_arc (context, pen, pt1, rx, ry, phi, fla, fls);  // Create the actual object out of lines and/or arcs, record the number added;

            pen[0] = pt1[0];                                                    // Update the current position to the new value;
            pen[1] = pt1[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'a':                                                               // >>> 'Relative elliptical arc':

          if (SVG_PARSE_ARC_DATA (rx, ry, phi, fla, fls, pt1, index))           // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 7);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] += pen[0];                                                   // Transform the relative values into absolute ones;
            pt1[1] += pen[1];

            items += gcode_svg_create_elliptic_arc (context, pen, pt1, rx, ry, phi, fla, fls);  // Create the actual object out of lines and/or arcs, record the number added;

            pen[0] = pt1[0];                                                    // Update the current position to the new value;
            pen[1] = pt1[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'Q':                                                               // >>> 'Absolute quadratic Bézier curve':

          if (SVG_PARSE_2X_POINT (pt1, pt2, index))                             // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 4);                                   // move the chunk parser pointer a corresponding amount ahead;

            items += gcode_svg_create_quadratic_bezier (context, pen, pt1, pt2);        // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt1[0];                                                    // Save the control point for the next potential shorthand;
            ptc[1] = pt1[1];
            pen[0] = pt2[0];                                                    // Update the current position to the new value;
            pen[1] = pt2[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'q':                                                               // >>> 'Relative quadratic Bézier curve':

          if (SVG_PARSE_2X_POINT (pt1, pt2, index))                             // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 4);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] += pen[0];                                                   // Transform the relative values into absolute ones;
            pt1[1] += pen[1];
            pt2[0] += pen[0];
            pt2[1] += pen[1];

            items += gcode_svg_create_quadratic_bezier (context, pen, pt1, pt2);        // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt1[0];                                                    // Save the control point for the next potential shorthand;
            ptc[1] = pt1[1];
            pen[0] = pt2[0];                                                    // Update the current position to the new value;
            pen[1] = pt2[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'T':                                                               // >>> 'Absolute quadratic Bézier curve (shorthand)':

          if (SVG_PARSE_1X_POINT (pt2, index))                                  // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 2);                                   // move the chunk parser pointer a corresponding amount ahead;

            if ((history == 'Q') || (history == 'T'))                           // If the last command was also a 'Q' or a 'T' (regardless of case),
            {
              pt1[0] = 2 * pen[0] - ptc[0];                                     // reflect the last command point on the start point to obtain the current one;
              pt1[1] = 2 * pen[1] - ptc[1];
            }
            else                                                                // If the last command was anything else,
            {
              pt1[0] = pen[0];                                                  // the current control point is considered to be the same as the start point;
              pt1[1] = pen[1];
            }

            items += gcode_svg_create_quadratic_bezier (context, pen, pt1, pt2);        // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt1[0];                                                    // Save the control point for the next potential shorthand;
            ptc[1] = pt1[1];
            pen[0] = pt2[0];                                                    // Update the current position to the new value;
            pen[1] = pt2[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 't':                                                               // >>> 'Relative quadratic Bézier curve (shorthand)':

          if (SVG_PARSE_1X_POINT (pt2, index))                                  // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 2);                                   // move the chunk parser pointer a corresponding amount ahead;

            if ((history == 'Q') || (history == 'T'))                           // If the last command was also a 'Q' or a 'T' (regardless of case),
            {
              pt1[0] = 2 * pen[0] - ptc[0];                                     // reflect the last command point on the start point to obtain the current one;
              pt1[1] = 2 * pen[1] - ptc[1];
            }
            else                                                                // If the last command was anything else,
            {
              pt1[0] = pen[0];                                                  // the current control point is considered to be the same as the start point;
              pt1[1] = pen[1];
            }

            pt2[0] += pen[0];                                                   // Transform the relative values into absolute ones;
            pt2[1] += pen[1];

            items += gcode_svg_create_quadratic_bezier (context, pen, pt1, pt2);        // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt1[0];                                                    // Save the control point for the next potential shorthand;
            ptc[1] = pt1[1];
            pen[0] = pt2[0];                                                    // Update the current position to the new value;
            pen[1] = pt2[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'C':                                                               // >>> 'Absolute cubic Bézier curve':

          if (SVG_PARSE_3X_POINT (pt1, pt2, pt3, index))                        // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 6);                                   // move the chunk parser pointer a corresponding amount ahead;

            items += gcode_svg_create_cubic_bezier (context, pen, pt1, pt2, pt3);       // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt2[0];                                                    // Save the second control point for the next potential shorthand;
            ptc[1] = pt2[1];
            pen[0] = pt3[0];                                                    // Update the current position to the new value;
            pen[1] = pt3[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'c':                                                               // >>> 'Relative cubic Bézier curve':

          if (SVG_PARSE_3X_POINT (pt1, pt2, pt3, index))                        // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 6);                                   // move the chunk parser pointer a corresponding amount ahead;

            pt1[0] += pen[0];                                                   // Transform the relative values into absolute ones;
            pt1[1] += pen[1];
            pt2[0] += pen[0];
            pt2[1] += pen[1];
            pt3[0] += pen[0];
            pt3[1] += pen[1];

            items += gcode_svg_create_cubic_bezier (context, pen, pt1, pt2, pt3);       // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt2[0];                                                    // Save the second control point for the next potential shorthand;
            ptc[1] = pt2[1];
            pen[0] = pt3[0];                                                    // Update the current position to the new value;
            pen[1] = pt3[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'S':                                                               // >>> 'Absolute cubic Bézier curve (shorthand)':

          if (SVG_PARSE_2X_POINT (pt2, pt3, index))                             // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 4);                                   // move the chunk parser pointer a corresponding amount ahead;

            if ((history == 'C') || (history == 'S'))                           // If the last command was also a 'C' or an 'S' (regardless of case),
            {
              pt1[0] = 2 * pen[0] - ptc[0];                                     // reflect the last command point on the start point to obtain the current one;
              pt1[1] = 2 * pen[1] - ptc[1];
            }
            else                                                                // If the last command was anything else,
            {
              pt1[0] = pen[0];                                                  // the current control point is considered to be the same as the start point;
              pt1[1] = pen[1];
            }

            items += gcode_svg_create_cubic_bezier (context, pen, pt1, pt2, pt3);       // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt2[0];                                                    // Save the second control point for the next potential shorthand;
            ptc[1] = pt2[1];
            pen[0] = pt3[0];                                                    // Update the current position to the new value;
            pen[1] = pt3[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 's':                                                               // >>> 'Relative cubic Bézier curve (shorthand)':

          if (SVG_PARSE_2X_POINT (pt2, pt3, index))                             // If a complete set of values could be read,
          {
            gcode_svg_seek_ahead (&index, 4);                                   // move the chunk parser pointer a corresponding amount ahead;

            if ((history == 'C') || (history == 'S'))                           // If the last command was also a 'C' or an 'S' (regardless of case),
            {
              pt1[0] = 2 * pen[0] - ptc[0];                                     // reflect the last command point on the start point to obtain the current one;
              pt1[1] = 2 * pen[1] - ptc[1];
            }
            else                                                                // If the last command was anything else,
            {
              pt1[0] = pen[0];                                                  // the current control point is considered to be the same as the start point;
              pt1[1] = pen[1];
            }

            pt2[0] += pen[0];                                                   // Transform the relative values into absolute ones;
            pt2[1] += pen[1];
            pt3[0] += pen[0];
            pt3[1] += pen[1];

            items += gcode_svg_create_cubic_bezier (context, pen, pt1, pt2, pt3);       // Create the actual object out of lines and/or arcs, record the number added;

            ptc[0] = pt2[0];                                                    // Save the control point for the next potential shorthand;
            ptc[1] = pt2[1];
            pen[0] = pt3[0];                                                    // Update the current position to the new value;
            pen[1] = pt3[1];
          }
          else                                                                  // If a complete set of values could NOT be read,
            gcode_svg_seek_ahead (&index, 0);                                   // move the parser pointer to the end of the chunk (abort parsing it further);

          break;

        case 'Z':
        case 'z':

          gcode_svg_seek_ahead (&index, 0);                                     // Move the parser pointer to the end of the chunk (abort parsing it further);

          GCODE_MATH_VEC2D_DIST (value, pen, start);                            // Find the distance between the current point and the starting point;

          if (value >= GCODE_PRECISION)                                         // If it is significant, close the path with a final line back to the start;
          {
            gcode_svg_create_line (context, pen, start);
          }

          pen[0] = start[0];                                                    // Restore the start position as the current one;
          pen[1] = start[1];

          break;

        default:                                                                // WHAT TRICKERY IS THIS?!? There ARE NO OTHER LETTERS that can start a chunk!

          gcode_svg_seek_ahead (&index, 0);                                     // Move the parser pointer to the end of the chunk (abort parsing it further);

          break;
      }

      if (history == 'Z')                                                       // If this is a newly started (sub)path, save the starting position;
      {
        start[0] = pen[0];                                                      // store the current position as the start;
        start[1] = pen[1];                                                      // worst case (if pen was not set this round either): it stays zero;
      }

      history = toupper (buffer[0]);                                            // Update the command history to the current command (as uppercase!);

    } while (*index);                                                           // If we're out of the loop, we parsed the current chunk as far as we could:

    chunk += length;                                                            // move on to the beginning of whatever is left after the current string chunk;
  }

  if (items > 0)                                                                // If there were any items successfully inserted into the new sketch,
    gcode_append_as_listtail (svg->parent_block, svg->sketch_block);            // append it to the end of 'parent_block's list (as head if the list is NULL)
  else                                                                          // If there were no items inserted into the sketch at all, 
    svg->sketch_block->free (&svg->sketch_block);                               // there's no point in keeping it - free it instead;

  free (buffer);                                                                // Finally, let's not forget freeing the path parsing buffer.
}

/**
 * The Expat "start tag" handler, looking for and parsing elements of interest;
 */

static void
svg_start (void *context, const char *tag, const char **attr)
{
  int i;
  gcode_svg_t *svg = (gcode_svg_t *) context;                                   // First things first: retrieve a reference to the SVG context;

  /* Look for element 'path' and attr 'd' for path data */

  if (strcmp (tag, SVG_XML_TAG_SVG) == 0)                                       // If this is a "svg" element, we're interested;
  {
    for (i = 0; attr[i]; i += 2)                                                // Iterate through the attributes looking for "width" or "height";
    {
      if (strcmp (attr[i], SVG_XML_ATTR_WIDTH) == 0)                            // Found a "width" attribute;
      {
        svg->size[0] = atof (attr[i + 1]);                                      // Try converting it: failure equals "0" returned - no harm there;

        if (svg->size[0])                                                       // If the numeric conversion failed, the rest would be pointless;
        {
          if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_PERCENT))               // If the native "unit" of the project is 'percent',
          {                                                                     // the material should not be resized based on that;
            svg->scale[0] = svg->size[0] / 100;                                 // So we update 'scale', but reset 'size' to zero.
            svg->size[0] = 0;
          }
          else if (svg->gcode->units == GCODE_UNITS_MILLIMETER)                 // If the native units of the project are millimeters...
          {
            if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_INCH))                // ...and the detected SVG units are inches,
            {
              svg->scale[0] = GCODE_INCH2MM;                                    // set an inches-to-mm conversion scale;
            }
            else if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_CM))             // ...and the detected SVG units are centimetres,
            {
              svg->scale[0] = GCODE_CM2MM;                                      // we still need to apply a cm-to-mm conversion;
            }
          }
          else if (svg->gcode->units == GCODE_UNITS_INCH)                       // If, on the other hand, the native units of the project are inches...
          {
            if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_MM))                  // ...and the detected SVG units are millimetres,
            {
              svg->scale[0] = GCODE_MM2INCH;                                    // set a mm-to-inches conversion scale;
            }
            else if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_CM))             // ...and the detected SVG units are centimetres,
            {
              svg->scale[0] = GCODE_CM2MM * GCODE_MM2INCH;                      // apply a compound cm-to-mm-to-inches conversion;
            }
          }
        }

        svg->size[0] *= svg->scale[0];                                          // Whatever the scale ended up to be, apply it to the newly detected width;
      }
      else if (strcmp (attr[i], SVG_XML_ATTR_HEIGHT) == 0)                      // Found a "height" attribute;
      {
        svg->size[1] = atof (attr[i + 1]);                                      // Try converting it: failure equals "0" returned - no harm there;

        if (svg->size[1])                                                       // If the numeric conversion failed, the rest would be pointless;
        {
          if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_PERCENT))               // If the native "unit" of the project is 'percent',
          {                                                                     // the material should not be resized based on that;
            svg->scale[1] = svg->size[1] / 100;                                 // So we update 'scale', but reset 'size' to zero.
            svg->size[1] = 0;
          }
          else if (svg->gcode->units == GCODE_UNITS_MILLIMETER)                 // If the native units of the project are millimeters...
          {
            if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_INCH))                // ...and the detected SVG units are inches,
            {
              svg->scale[1] = GCODE_INCH2MM;                                    // set an inches-to-mm conversion scale;
            }
            else if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_CM))             // ...and the detected SVG units are centimetres,
            {
              svg->scale[1] = GCODE_CM2MM;                                      // we still need to apply a cm-to-mm conversion;
            }
          }
          else if (svg->gcode->units == GCODE_UNITS_INCH)                       // If, on the other hand, the native units of the project are inches...
          {
            if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_MM))                  // ...and the detected SVG units are millimetres,
            {
              svg->scale[1] = GCODE_MM2INCH;                                    // set a mm-to-inches conversion scale;
            }
            else if (strstr ((char *)attr[i + 1], SVG_XML_UNIT_CM))             // ...and the detected SVG units are centimetres,
            {
              svg->scale[1] = GCODE_CM2MM * GCODE_MM2INCH;                      // apply a compound cm-to-mm-to-inches conversion;
            }
          }
        }

        svg->size[1] *= svg->scale[1];                                          // Whatever the scale ended up to be, apply it to the newly detected height;
      }
    }
  }
  else if (strcmp (tag, SVG_XML_TAG_PATH) == 0)                                 // If this is a "path" element, we're interested;
  {
    for (i = 0; attr[i]; i += 2)                                                // Iterate through the attributes looking for "d" - the actual path data;
    {
      if (strcmp (attr[i], SVG_XML_ATTR_PATH_DATA) == 0)                        // Found a "d" attribute;
      {
        gcode_svg_parse_path_data (context, (char *)(attr[i + 1]));             // Parsing that is a bit of a mouthful - let's learn to delegate...
      }
    }
  }
}

/**
 * The Expat "end tag" handler, very keen to help but with very little to do;
 */

static void
svg_end (void *data, const char *el)
{
}

/**
 * Open 'filename' and import its contents then insert it under 'template';
 * Automatic unit conversion is performed if the detected unit in the SVG is not
 * the same as the native unit of the project (only "mm", "cm" and "in" handled,
 * so any other unit or none at all implies dimension values get imported 1:1).
 */

int
gcode_svg_import (gcode_block_t *template_block, char *filename)
{
  gcode_svg_t svg;

  FILE *fh;
  long int length, nomore;
  char *buffer;

  XML_Parser parser = XML_ParserCreate ("UTF-8");

  svg.gcode = template_block->gcode;                                            // Initialize the SVG context used by the parser callback;
  svg.parent_block = template_block;                                            // Everything we import goes under this block;
  svg.sketch_block = NULL;
  svg.size[0] = 0.0;                                                            // Hopefully, this will get updated to the width the SVG is supposed to have;
  svg.size[1] = 0.0;                                                            // Equally, if the parser detects a height tag, this is where the value goes;
  svg.scale[0] = 1.0;                                                           // If the values above carry a unit, the ratio from it to the project's native
  svg.scale[1] = 1.0;                                                           // unit goes into this pair, so all imported dimensions get suitably rescaled;

  if (!parser)                                                                  // Check if we have an Expat parser - if not, bail;
  {
    REMARK ("Failed to allocate memory for XML parser\n");
    return (1);
  }

  XML_SetElementHandler (parser, svg_start, svg_end);                           // Set the element handlers for the Expat parser;
  XML_SetUserData (parser, (void *)&svg);                                       // The data to be passed to them is a reference to our SVG context;

  fh = fopen (filename, "r");

  if (!fh)                                                                      // If opening the file failed, bail;
  {
    REMARK ("Failed to open file '%s'\n", basename (filename));
    XML_ParserFree (parser);
    return (1);
  }

  fseek (fh, 0, SEEK_END);                                                      // Read the entire file into the newly allocated 'buffer';
  length = ftell (fh);

  if (!length)                                                                  // A zero length file is not an error, strictly speaking;
  {
    XML_ParserFree (parser);
    fclose (fh);
    return (0);
  }

  buffer = malloc (length);

  if (!buffer)
  {
    REMARK ("Failed to allocate memory for SVG import buffer\n");
    XML_ParserFree (parser);
    fclose (fh);
    return (1);
  }

  fseek (fh, 0, SEEK_SET);
  nomore = fread (buffer, 1, length, fh);
  fclose (fh);

  if (XML_Parse (parser, buffer, nomore, 1) == XML_STATUS_ERROR)                // Try to feed all of it to Expat - if it squeals, bail;
  {
    REMARK ("XML parse error in file '%s' at line %d: %s\n", basename (filename), (int)XML_GetCurrentLineNumber (parser), XML_ErrorString (XML_GetErrorCode (parser)));
    XML_ParserFree (parser);
    free (buffer);
    return (1);
  }

  XML_ParserFree (parser);                                                      // If we're still here, it worked - time to clean up, starting with the parser;
  free (buffer);

  if (svg.gcode->material_size[0] < svg.size[0])                                // If the imported stuff is wider than the project material, embiggen that;
    svg.gcode->material_size[0] = svg.size[0];

  if (svg.gcode->material_size[1] < svg.size[1])                                // Do the same if the project material is not tall enough;
    svg.gcode->material_size[1] = svg.size[1];

  gcode_prep (svg.gcode);

  return (0);
}
