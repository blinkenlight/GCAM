/**
 *  gcode_sketch.c
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

#include "gcode_sketch.h"
#include "gcode_extrusion.h"
#include "gcode_tool.h"
#include "gcode_pocket.h"
#include "gcode_arc.h"
#include "gcode_line.h"
#include "gcode.h"

#define SHARPNESS_LIMIT       -0.5

/**
 * Return -1 if the "inside" of the closed curve 'start_block' -> 'end_block' is
 * to the right of the curve, or +1 if the "inside" of the curve is to the left;
 * Intuitively, that is the same thing as saying the the curve is mostly bending
 * in the trigonometric negative direction ("to the right") if the result is -1
 * or in the trigonometric positive direction ("to the left") if +1 results;
 * It is assumed the chain of blocks starting with 'start_block' and ending with
 * 'end_block' is a contiguous closed contour that does not intersect itself. As
 * following such a curve would result in sweeping EXACTLY positive or negative
 * 360 degrees, adding up direction changes from block to block including angles
 * sweeped by arcs, then simply comparing to zero should yield the answer.
 */

static gfloat_t
gcode_sketch_inside (gcode_block_t *start_block, gcode_block_t *end_block)
{
  gcode_block_t *index_block;
  gfloat_t first_angle;
  gfloat_t prior_angle;
  gfloat_t delta_angle;
  gfloat_t swept_angle;

  swept_angle = 0;                                                              // This is the total angle a tangent following the curve would sweep: set to 0;

  index_block = start_block;                                                    // Starting with 'start_block',

  while (index_block != end_block->next)                                        // iterate along the block chain until 'end_block' is reached;
  {
    switch (index_block->type)                                                  // Sketches can only possibly contain lines or arcs;
    {
      case GCODE_TYPE_LINE:                                                     // If this block is a line,
      {
        gcode_line_t *line;

        gfloat_t x0, y0, x1, y1;
        gfloat_t slope_angle;

        line = (gcode_line_t *)index_block->pdata;                              // get a reference to the line-specific data structure,

        x0 = line->p0[0];                                                       // then retrieve the coordinates of the endpoints.
        y0 = line->p0[1];

        x1 = line->p1[0];
        y1 = line->p1[1];

        slope_angle = GCODE_RAD2DEG * atan2 (y1 - y0, x1 - x0);                 // This should give us the angle of the line's slope, as -180...180 degrees;

        if (index_block == start_block)                                         // If this is the first block,
        {
          first_angle = slope_angle;                                            // just save that value - we'll need it to close up our 'sweep' at the end;
        }
        else                                                                    // If this is not the first block,
        {
          delta_angle = slope_angle - prior_angle;                              // find how much (and which way!) we turned from the previous block;

          GCODE_MATH_WRAP_SIGNED_DEGREES (delta_angle);                         // This is needed to keep the result in the signed range -180...180 degrees;

          swept_angle = swept_angle + delta_angle;                              // Take the amount we turned and add it to the global sum.
        }

        prior_angle = slope_angle;                                              // Save the current 'heading' as the reference to compare the next section to.

        break;
      }

      case GCODE_TYPE_ARC:                                                      // If this block is an arc,
      {
        gcode_arc_t *arc;

        gfloat_t start_angle;
        gfloat_t sweep_angle;
        gfloat_t enter_angle;
        gfloat_t leave_angle;

        arc = (gcode_arc_t *)index_block->pdata;                                // get a reference to the arc-specific data structure,

        start_angle = arc->start_angle;                                         // then retrieve the start and sweep angles of the arc.
        sweep_angle = arc->sweep_angle;

        enter_angle = sweep_angle < 0 ? start_angle - 90 : start_angle + 90;    // This is the direction of the tangent at the start of the arc;

        GCODE_MATH_WRAP_SIGNED_DEGREES (enter_angle);                           // This is needed to keep the result in the signed range -180...180 degrees;

        leave_angle = enter_angle + sweep_angle;                                // This is the direction of the tangent at the end of the arc;

        GCODE_MATH_WRAP_SIGNED_DEGREES (leave_angle);                           // This is needed to keep the result in the signed range -180...180 degrees;

        if (index_block == start_block)                                         // If this is the first block,
        {
          first_angle = enter_angle;                                            // save the initial heading (we'll need it to close up our 'sweep' at the end),

          swept_angle = swept_angle + sweep_angle;                              // then add the angle the arc sweeps to the global sum.
        }
        else                                                                    // If this is not the first block,
        {
          delta_angle = enter_angle - prior_angle;                              // find how much (and which way!) we turned from the previous block;

          GCODE_MATH_WRAP_SIGNED_DEGREES (delta_angle);                         // This is needed to keep the result in the signed range -180...180 degrees;

          swept_angle = swept_angle + delta_angle + sweep_angle;                // Now add the angle we turned plus the angle the arc sweeps to the global sum.
        }

        prior_angle = leave_angle;                                              // Save the current 'heading' as the reference to compare the next section to.

        break;
      }
    }

    index_block = index_block->next;                                            // Move on to the next block.
  }

  if (start_block != end_block)                                                 // If there was more than one block, close the sweep from the last to the first;
  {
    delta_angle = first_angle - prior_angle;                                    // Calculate the amount turned the usual way,

    GCODE_MATH_WRAP_SIGNED_DEGREES (delta_angle);                               // wrap the result into the -180...180 degrees range,

    swept_angle = swept_angle + delta_angle;                                    // then add it to the total angle swept.
  }

  if (swept_angle < 0)                                                          // The result HAS to be EITHER +360 OR -360, but we can just check the sign;
    return (-1.0);                                                              // If we kept turning RIGHT (trigonometric '-'), 'inside' is on the right;
  else
    return (+1.0);                                                              // If we kept turning LEFT (trigonometric '+'), 'inside' is on the left.
}

/**
 * Using block flags, keep looking for a NOT tagged block starting with 'block'
 * itself and return the first one found. If 'listhead' is null stop at the end
 * of the chain, otherwise loop around continuing from the block pointed at by
 * 'listhead'. If 'block' itself is null, consider the end of the list reached
 * and act according to the content of 'listhead'. If either the end of the list
 * is reached or a complete loop-around happens back to 'block' before a tagfree
 * block is found, null is returned; else, a pointer to the tagfree block found.
 * NOTE: the reason why this doesn't simply use 'prev' links to find the head is
 * that this method offers an easy way to select circular or single traversal.
 * NOTE: indeed, if 'block' is not part of the chain 'listhead' is pointing at,
 * this can end rather badly; so, y'know, how about don't do that.
 * NOTE: the reason a null starting block is allowed is so that this can also be
 * used to find the NEXT block after the current one - clearly, in such a case
 * 'next' can easily be null, and we have to deal with that case.
 */

static gcode_block_t *
gcode_sketch_first_untagged (gcode_block_t *block, gcode_block_t *listhead)
{
  gcode_block_t *index_block;

  index_block = block;

  for (;;)                                                                      // Can't loop on tagged-ness before we check for null-ness
  {
    if (index_block)                                                            // If the block exists, we can now test if it's tagged;
    {
      if (!GCODE_UTIL_IS_TAGGED (index_block))                                  // If it's not, we're done - return it as-is;
        return (index_block);
      else                                                                      // If it is, move on the the next block;
        index_block = index_block->next;
    }
    else                                                                        // If block points to nothing, loop over or stop;
    {
      if (listhead)                                                             // If there is a listhead, loop over to it and keep going;
        index_block = listhead;
      else                                                                      // If listhead points to nothing, the list does not loop - give up;
        return (NULL);
    }

    if (index_block == block)                                                   // If the list loops but has no non-tagged blocks we would be stuck here,
      return (NULL);                                                            // so if we loop around back to the original block, make sure we give up.
  }

  return (NULL);                                                                // Getting here requires either magic or corrupted code...
}

/**
 * Keep crawling backwards on the block chain starting with 'block' itself and
 * return the first block which has a starting point further away than 'radius'
 * distance from 'datum'. If 'listtail' is null stop at the start of the chain,
 * otherwise wrap around continuing from the block pointed at by 'listtail'.
 * If either the start of the list is reached or a complete wrap-around happens
 * back to 'block' before a suitable block is found, the current block shall be
 * returned; else, a pointer to the block found.
 * NOTE: this is a "soft" search in the sense that the block returned does not
 * necessarily satisfy the radius requirement, it's only a "best effort" until
 * the relevant limit is reached and the search has to stop;
 * NOTE: the reason why this doesn't simply use 'next' links to find the end is
 * that this method offers an easy way to select circular or single traversal.
 * NOTE: indeed, if 'block' is not part of the chain 'listtail' is pointing at,
 * this can end rather badly; so, y'know, how about don't do that.
 * NOTE: 'block' may not be null, it needs to be an actual line or arc block.
 */

static gcode_block_t *
gcode_sketch_rev_first_outside (gcode_block_t *block, gcode_block_t *listtail, gcode_vec2d_t datum, gfloat_t radius)
{
  gcode_block_t *index_block, *next_block;
  gcode_vec2d_t point;

  if (!block)                                                                   // This routine does not allow a null current block;
    return (NULL);

  index_block = block;                                                          // Start with 'block' itself;

  for (;;)                                                                      // Just keep looping until something trips;
  {
    gcode_util_endpoint(index_block, point, GCODE_GET_ALPHA);                   // Get the starting point of 'index_block' as 'point';

    if (GCODE_MATH_2D_DISTANCE (datum, point) > radius)                         // Is it outside 'radius'? If it is, return this block;
      return (index_block);

    if (index_block->prev)                                                      // If the list continues, select 'prev' as the next block;
      next_block = index_block->prev;
    else if (listtail)                                                          // If it does not, but 'listtail' is not null, wrap to it;
      next_block = listtail;
    else                                                                        // If 'listtail' is null, give up and return this block;
      return (index_block);

    if (next_block == block)                                                    // If a looping list has no blocks outside 'radius' we'd be stuck here,
      return (index_block);                                                     // so if we loop around back to the original block, make sure we give up.

    index_block = next_block;                                                   // If we're still here, move on to the selected next block;
  }

  return (block);                                                               // Getting here requires either magic or corrupted code...
}

/**
 * Keep crawling forward on the block chain starting with 'block' itself and
 * return the first block which has an ending point further away than 'radius'
 * distance from 'datum'. If 'listhead' is null stop at the end of the chain,
 * otherwise wrap around continuing from the block pointed at by 'listhead'.
 * If either the end of the list is reached or a complete wrap-around happens
 * back to 'block' before a suitable block is found, the current block shall be
 * returned; else, a pointer to the block found.
 * NOTE: this is a "soft" search in the sense that the block returned does not
 * necessarily satisfy the radius requirement, it's only a "best effort" until
 * the relevant limit is reached and the search has to stop;
 * NOTE: the reason why this doesn't simply use 'prev' links to find the end is
 * that this method offers an easy way to select circular or single traversal.
 * NOTE: indeed, if 'block' is not part of the chain 'listhead' is pointing at,
 * this can end rather badly; so, y'know, how about don't do that.
 * NOTE: 'block' may not be null, it needs to be an actual line or arc block.
 */

static gcode_block_t *
gcode_sketch_fwd_first_outside (gcode_block_t *block, gcode_block_t *listhead, gcode_vec2d_t datum, gfloat_t radius)
{
  gcode_block_t *index_block, *next_block;
  gcode_vec2d_t point;

  if (!block)                                                                   // This routine does not allow a null current block;
    return (NULL);

  index_block = block;                                                          // Start with 'block' itself;

  for (;;)                                                                      // Just keep looping until something trips;
  {
    gcode_util_endpoint(index_block, point, GCODE_GET_OMEGA);                   // Get the ending point of 'index_block' as 'point';

    if (GCODE_MATH_2D_DISTANCE (datum, point) > radius)                         // Is it outside 'radius'? If it is, return this block;
      return (index_block);

    if (index_block->next)                                                      // If the list continues, select 'next' as the next block;
      next_block = index_block->next;
    else if (listhead)                                                          // If it does not, but 'listhead' is not null, wrap to it;
      next_block = listhead;
    else                                                                        // If 'listhead' is null, give up and return this block;
      return (index_block);

    if (next_block == block)                                                    // If a looping list has no blocks outside 'radius' we'd be stuck here,
      return (index_block);                                                     // so if we loop around back to the original block, make sure we give up.

    index_block = next_block;                                                   // If we're still here, move on to the selected next block;
  }

  return (block);                                                               // Getting here requires either magic or corrupted code...
}

/**
 * Take each block in the list starting with 'listhead' and try intersecting it
 * with the block that follows; if at least one intersection that falls between
 * the endpoints of both primitives (line/arc) is found, both are shortened to
 * that intersection point (or the closer one if more than one was found); if
 * 'closed' is true, the last block 'wraps around' to the first, otherwise not;
 * NOTE: the supplied list is expected to consist of a chain blocks that used to
 * be connected originally (before extrusion offsets were applied) - trying to
 * intersect randomly ordered blocks is unlikely to result in anything sane;
 * NOTE: the supplied list is expected to already be tagged (using block flags)
 * so that any primitives of zero length zero radius or zero sweep are flagged;
 * NOTE: unfortunate as it may be, this RELIES on all involved blocks having an
 * offset of ZERO, considering intersection calculates taking any offsets into
 * account, but obtained intersection points are then assigned as new endpoints
 * DIRECTLY, without trying to apply to them any of those offsets "in reverse";
 * As it is, it's the CALLER's responsibility to make sure there are NO OFFSETS.
 */

static void
gcode_sketch_trim_intersections (gcode_block_t *listhead, int closed)
{
  gcode_block_t *index_block, *next_block;
  gcode_vec2d_t ip_array[2];
  int ip_count, ip_index;

  if (!listhead)                                                                // The list should not be empty, but if it is, leave;
    return;

  index_block = gcode_sketch_first_untagged (listhead, NULL);                   // Start with the first non-tagged block in the list;

  while (index_block)                                                           // Keep looping through all the blocks in the list;
  {
    if (closed)                                                                 // If the chain is supposed to be closed,
      next_block = gcode_sketch_first_untagged (index_block->next, listhead);   // loop around at the end while looking for the next block;
    else                                                                        // If the chain is supposed to be open,
      next_block = gcode_sketch_first_untagged (index_block->next, NULL);       // stop looking for the next block having reached the end;

    if (!next_block)                                                            // If no suitable next block has been found,
      break;                                                                    // we're done intersecting - just leave;

    gcode_util_intersect (index_block, next_block, ip_array, &ip_count);        // Get an array of intersection points between 'index_block' and 'next_block';

    if (ip_count > 0)                                                           // This only makes sense if at least one such point was found;
    {
      if (ip_count > 1)                                                         // If multiple intersections exist (well, maximum two), let's pick one;
      {
        gcode_vec2d_t p;
        gfloat_t d0, d1;

        gcode_util_endpoint(index_block, p, GCODE_GET_ALPHA);                   // Obtain the far endpoint of the first primitive,

        d0 = GCODE_MATH_2D_DISTANCE (p, ip_array[0]);                           // then calculate the distance from its ending point to both intersections;
        d1 = GCODE_MATH_2D_DISTANCE (p, ip_array[1]);

        ip_index = (d0 < d1) ? 0 : 1;                                           // The one that is closer gets chosen;
      }
      else
      {
        ip_index = 0;                                                           // If there's only one intersection point, it wins by default;
      }

      gcode_util_trim_both (index_block, next_block, ip_array[ip_index]);
    }

    index_block = gcode_sketch_first_untagged (index_block->next, NULL);        // Move on to the next non-tagged block, WITHOUT looping past the end of the list;
  }
}

/**
 * Take each block in the list starting with 'listhead' and see if its end-point
 * coincides with the start point of the block that follows it; if not, attempt
 * to build and insert a transition arc to bridge the gap between the endpoints;
 * NOTE: the supplied list is expected to consist of a chain blocks that used to
 * be connected originally (before extrusion offsets were applied) upon which an
 * attempt to 'intersect and trim' has already been made, so only the primitives
 * that don't intersect remain unconnected - trying to build transitions between
 * randomly ordered blocks is unlikely to result in anything sane;
 * NOTE: the supplied list is expected to already be tagged (using block flags)
 * so that any primitives of zero length zero radius or zero sweep are flagged;
 * NOTE: the transition arcs attempt to maintain 'C1 continuity' which is just a
 * fancy way to say that the arc should transition 'smoothly' on both ends into
 * the primitives it connects (both should be tangential to the arc); this use
 * case should always allow that, but a generalized situation can easily have no
 * possible solution - if so, a line is inserted to bridge the gap instead;
 * NOTE: while this *might possibly* work with arbitrary offsets if all involved
 * methods manage to agree on whether to use them or not, the current use case
 * assumes all blocks involved are linked to an offset of ZERO, end of story:
 * As it is, it's the CALLER's responsibility to make sure there are NO OFFSETS.
 * NOTE: for the curious, the math used is based on the simple realization that
 * if the arc is to be tangential in both its endpoints to whatever it connects,
 * its radii must conversely be normal to its 'neighbors' at both ends, meaning
 * that the point where those normals intersect HAS TO BE our arc's center; and
 * since we CAN fairly easily get & intersect those normals, we're home free...
 */

static void
gcode_sketch_insert_transitions (gcode_block_t *listhead, int closed)
{
  gcode_block_t *index_block, *next_block, *new_block;
  gcode_vec2d_t p0, p1, p, n0, n1, n;
  gfloat_t d0, d1, d, a0, a1, a, b, f;

  if (!listhead)                                                                // The list should not be empty, but if it is, leave;
    return;

  index_block = listhead;                                                       // Start with the first block in the list;

  while (index_block)                                                           // Keep looping through all the blocks in the list;
  {
    /**
    /* If there are any null-size blocks between this one and the next one that
    /* can be skipped we MUST skip them or else we get aberrant transition arcs
     */

    if (closed)                                                                 // If the chain is supposed to be closed,
      next_block = gcode_sketch_first_untagged (index_block->next, listhead);   // loop around at the end while looking for the next block;
    else                                                                        // If the chain is supposed to be open,
      next_block = gcode_sketch_first_untagged (index_block->next, NULL);       // stop looking for the next block having reached the end;

    if (next_block)                                                             // If a suitable NON-NULL next block has been found,
    {
      index_block->ends (index_block, p, p0, GCODE_GET);                        // Obtain the endpoints of both primitives;
      next_block->ends (next_block, p1, p, GCODE_GET);

      d = GCODE_MATH_2D_DISTANCE (p0, p1);                                      // Find the distance from the end of this block to the start of the next;

      if (d < GCODE_TOLERANCE)                                                  // If that distance is zero, there cannot be a transition between them.
      {
        index_block = gcode_sketch_first_untagged (index_block->next, NULL);    // So skip any null blocks between this one and the next WITHOUT LOOPING,

        continue;                                                               // abandon this transition and move on to the next pair of blocks.
      }
    }

    /**
    /* If we are still here we have to try transitioning from this block to the
    /* immediately next one regardless of whether it is a null-sized one or not
     */

    if (index_block->next)                                                      // If this is not the last block,
      next_block = index_block->next;                                           // the one we want to try transition it to is the next block;
    else if (closed)                                                            // If this IS the last block but the list is 'closed',
      next_block = listhead;                                                    // the one we want to try transition it to is the FIRST block;
    else                                                                        // If this is the last block and the list in NOT 'closed',
      break;                                                                    // we're done transitioning - just leave;

    index_block->ends (index_block, p, p0, GCODE_GET);                          // Obtain the endpoints of both primitives;
    next_block->ends (next_block, p1, p, GCODE_GET);

    d = GCODE_MATH_2D_DISTANCE (p0, p1);                                        // Find the distance between the endpoints that should be connected;

    if (d < GCODE_TOLERANCE)                                                    // If that distance is zero, there cannot be a transition between them.
    {
      index_block = index_block->next;                                          // So abandon this transition and move on to the next pair of blocks.

      continue;
    }

    /**
    /* If we are still here there is an actual transition needed here: find one
     */

    new_block = NULL;                                                           // Clear the reference that should point to the new transition arc;

    index_block->ends (index_block, n, n0, GCODE_GET_NORMAL);                   // Obtain NORMAL VECTORS to the endpoints of both primitives; since a radius is
    next_block->ends (next_block, n1, n, GCODE_GET_NORMAL);                     // always normal to an arc, these normals MUST BE our radii for C1 continuity;
                                                                                // That means we only need to find their intersection to get our arc's center!
    a = n0[1] * n1[0] - n0[0] * n1[1];                                          // This is the denominator of a factor that will get us the intersection point;

    if (fabs (a) > GCODE_PRECISION)                                             // Obviously, if it's zero, we have no intersection (the normals are parallel).
    {
      b = (p0[0] - p1[0]) * n0[1] - (p0[1] - p1[1]) * n0[0];                    // The rest of the formula for the intersection factor - this is the numerator;

      f = b / a;                                                                // This factor is what one needs to multiply the unity-sized normal with
                                                                                // to get from point 'p1' to the point where the normals intersect.
      p[0] = p1[0] + f * n1[0];                                                 // Note: 'f' results from solving equations asserting that multiplying 'n0' by
      p[1] = p1[1] + f * n1[1];                                                 // some factor 'e' and 'n1' by some factor 'f' gets you to the same point 'p';

      d0 = GCODE_MATH_2D_DISTANCE (p0, p);                                      // Find the distance of both endpoints 'p0', 'p1' from the proposed transition
      d1 = GCODE_MATH_2D_DISTANCE (p1, p);                                      // arc center point 'p': clearly, if they differ, this arc is a bust...

      if (fabs (d1 - d0) < GCODE_PRECISION)                                     // If the two proposed 'radii' are effectively equal, this is a viable arc.
      {
        gcode_arc_t *arc;

        gcode_math_xy_to_angle (p, p0, &a0);                                    // There is actually not enough information in endpoints and center alone to
        gcode_math_xy_to_angle (p, p1, &a1);                                    // reliably calculate the correct sweep of an arc in general...

        gcode_arc_init (&new_block, index_block->gcode, NULL);

        arc = (gcode_arc_t *)new_block->pdata;

        arc->p[0] = p0[0];
        arc->p[1] = p0[1];

        arc->radius = d0;

        arc->start_angle = a0;
        arc->sweep_angle = a1 - a0;                                             // ...so this only works because the sweep angle of a _transition_ arc...

        GCODE_MATH_WRAP_SIGNED_DEGREES (arc->sweep_angle);                      // ...is not supposed to be larger than 180 degrees.
      }
    }
    else                                                                        // Obviously, obvious things never are - collinear normals can still intersect.
    {
      gcode_vec2d_t t0, t1, t, v;

      GCODE_MATH_VEC2D_SUB (v, p1, p0);                                         // So if the line connecting the two points of interest...
      GCODE_MATH_VEC2D_UNITIZE (v);
      GCODE_MATH_VEC2D_DOT (f, v, n1);                                          // ...is ALSO parallel with the normals, we can still form a transition arc;

      if (fabs (f) - 1 < GCODE_PRECISION)
      {
        gcode_arc_t *arc;

        p[0] = (p0[0] + p1[0]) / 2;
        p[1] = (p0[1] + p1[1]) / 2;

        d = GCODE_MATH_2D_DISTANCE (p0, p1) / 2;

        gcode_math_xy_to_angle (p, p0, &a0);                                    // Fortunately, there's no need to calculate the sweep: it can only be 180 degrees;

        gcode_arc_init (&new_block, index_block->gcode, NULL);

        arc = (gcode_arc_t *)new_block->pdata;

        arc->p[0] = p0[0];
        arc->p[1] = p0[1];

        arc->radius = d;

        arc->start_angle = a0;
        arc->sweep_angle = 180;                                                 // Unfortunately, we have no idea whether it's supposed to be +180 or -180 degrees.

        new_block->ends (new_block, t0, t1, GCODE_GET_TANGENT);                 // So we take our best shot at tangency at whichever end we manage to compute it;

        if (index_block->ends (index_block, v, t, GCODE_GET_TANGENT) == 0)
        {
          GCODE_MATH_VEC2D_DOT (f, t, t0);

          if (f < 0)
            arc->sweep_angle = -arc->sweep_angle;
        }
        else if (next_block->ends (next_block, t, v, GCODE_GET_TANGENT) == 0)
        {
          GCODE_MATH_VEC2D_DOT (f, t1, t);

          if (f < 0)
            arc->sweep_angle = -arc->sweep_angle;
        }
      }
    }

    if (!new_block)                                                             // If the transition arc is still nonexistent, it seems we can't build one;
    {
      gcode_line_t *line;

      gcode_line_init (&new_block, index_block->gcode, NULL);                   // So as a stopgap last effort, we insert a line from 'p0' to 'p1' instead.

      line = (gcode_line_t *)new_block->pdata;
                                                                                // Note: in current usage this should never happen though - as long as the
      line->p0[0] = p0[0];                                                      // transition arcs are used to 'plug up' gaps created by extrusion profile
      line->p0[1] = p0[1];                                                      // displacement (obviously equal for both primitives here), there always is
      line->p1[0] = p1[0];                                                      // a possible transition arc - rather amusingly, we just keep recalculating
      line->p1[1] = p1[1];                                                      // as our 'p' the points where the original un-offset primitives meet up...
    }

    gcode_insert_after_block (index_block, new_block);                          // Either way, *SOME* transition block MUST exist by now: insert it, THE END.

    index_block = index_block->next;                                            // We have a brand new block in front of us now, so we need to advance TWICE;

    index_block = index_block->next;                                            // Move on to the next pair; can't use 'next_block', it may have been looping.
  }
}

/**
 * Take each block in the list starting with 'listhead' and see if its end-point
 * and the start point of the block that follows it form a sharp spike; if they
 * do, that is usually (but not always) a sign that the contour is attempting to
 * execute a "three point turn" (or indeed a 101-point one) somewhere it should
 * only be doing a simple one, screwing up the part in the process. Try locating
 * the limits of the "spiky" region and look for the self-intersection that is
 * usually caused nearby, then remove the blocks between the intersecting ones.
 * NOTE: the supplied list is expected to consist of a chain blocks that is now
 * offset and re-connected into a contiguous chain by the previous passes;
 * NOTE: the supplied list is expected to not contain primitives of zero length
 * zero radius or zero sweep - those should be eliminated on some earlier pass;
 * NOTE: while this *might possibly* work with arbitrary offsets if all involved
 * methods manage to agree on whether to use them or not, the current use case
 * assumes all blocks involved are linked to an offset of ZERO, end of story:
 * As it is, it's the CALLER's responsibility to make sure there are NO OFFSETS.
 */

static void
gcode_sketch_check_sharp_points (gcode_block_t **listhead, int closed)
{
  gcode_block_t *head, *tail;
  gcode_block_t *index_block, *focus_block;
  gcode_block_t *incoming_block, *outgoing_block;
  gcode_sketch_t *sketch;
  gcode_vec2d_t datum;
  gfloat_t radius, index;

  head = *listhead;

  if (!head)                                                                    // The list should not be empty, but if it is, leave;
    return;

  if (!head->next)                                                              // The list should not be one block: if it is, leave;
    return;

  if (head->parent->type != GCODE_TYPE_SKETCH)                                  // If the parent of these blocks isn't a sketch, leave;
    return;

  sketch = (gcode_sketch_t *)head->parent->pdata;                               // Acquire a reference to the sketch data of the parent block;

  radius = (sketch->offset.eval + sketch->offset.tool) * 2;                     // Establish the radius of interest around a sharp point;

  if (radius < GCODE_PRECISION)                                                 // If there is no offset present in this contour, leave;
    return;

  index_block = head;                                                           // This block will be looking for the "incoming" segments;

  GCODE_UTIL_CLEAR_TAG (index_block);                                           // Clear any tagging the first block may have had;

  while (index_block -> next)                                                   // First of all run all the way to the end of the chain;
  {
    index_block = index_block->next;
    GCODE_UTIL_CLEAR_TAG (index_block);                                         // And clear any tagging any of the blocks may have had;
  }

  tail = index_block;                                                           // Keep a pointer to the last block because of course we do;
  index_block = head;                                                           // Start with the first block in the list;

  while (index_block)                                                           // Keep crawling forward through all the blocks in the list;
  {
    if (index_block->next)                                                      // If this is not the last block,
      focus_block = index_block->next;                                          // the one we want to compare it to is the next block;
    else if (closed)                                                            // If this IS the last block but the list is 'closed',
      focus_block = head;                                                       // the one we want to compare it to is the FIRST block;
    else                                                                        // If this is the last block and the list in NOT 'closed',
      break;                                                                    // we're done  - just leave;

    if (!GCODE_UTIL_IS_TAGGED (index_block) &&
        !GCODE_UTIL_IS_TAGGED (focus_block))                                    // If either block is already tagged, they should be skipped;
    {
      gcode_util_get_continuity_index (index_block, focus_block, &index);       // Figure out how sharp an angle these two blocks connect at;

      if (index < SHARPNESS_LIMIT)                                              // If the answer is "very", this is now a point of interest;
      {
        gcode_util_endpoint (focus_block, datum, GCODE_GET_ALPHA);              // The sharp point is the first endpoint of the second block;

        if (closed)                                                             // Seek blocks on both sides of this point that end farther than 'radius' from it,
        {
          incoming_block = gcode_sketch_rev_first_outside (index_block, tail, datum, radius);
          outgoing_block = gcode_sketch_fwd_first_outside (focus_block, head, datum, radius);
        }
        else                                                                    // either wrapping around the ends of the list or not, as appropriate;
        {
          incoming_block = gcode_sketch_rev_first_outside (index_block, NULL, datum, radius);
          outgoing_block = gcode_sketch_fwd_first_outside (focus_block, NULL, datum, radius);
        }

        /**
         * From this point on, everything can be done circularly as we know that
         * neither 'incoming_block' nor 'outgoing_block' went back and forward
         * farther than it was supposed to, depending on the chain being closed
         * or not; therefore there is no need to further test for that while we
         * are iterating between those two blocks looking for an intersection;
         */

        focus_block = incoming_block;                                           // This reference now changes purpose: it will shuttle back and forth
                                                                                // on the incoming side, looking for an intersection with the other side;
        for (;;)                                                                // Keep looping until an intersection is found or nothing remains to test;
        {
          gcode_vec2d_t ip_array[2];
          int ip_count, ip_index;

          gcode_util_intersect (focus_block, outgoing_block, ip_array, &ip_count);      // Try intersecting 'focus' with the current 'outgoing' block;

          if (ip_count > 0)                                                     // If an intersection with 'outgoing' HAS been found,
          {
            incoming_block = focus_block;                                       // then the real 'incoming' block is the current 'focus';

            if (ip_count > 1)                                                   // If multiple intersections exist (well, maximum two), let's pick one;
            {
              gcode_vec2d_t p;
              gfloat_t d0, d1;

              gcode_util_endpoint(incoming_block, p, GCODE_GET_ALPHA);          // Obtain the far endpoint of the 'incoming' block,

              d0 = GCODE_MATH_2D_DISTANCE (p, ip_array[0]);                     // then calculate the distance from it to both intersections;
              d1 = GCODE_MATH_2D_DISTANCE (p, ip_array[1]);

              ip_index = (d0 < d1) ? 0 : 1;                                     // The one that is closer (gets shortest result) gets chosen;
            }
            else
            {
              ip_index = 0;                                                     // If there's only one intersection point, it wins by default;
            }

            gcode_util_trim_both(incoming_block, outgoing_block, ip_array[ip_index]);   // Trim both intersecting segments back to the intersection point;

            focus_block = incoming_block;                                       // This reference changes purpose again: it will now mop up any blocks
            gcode_get_circular_next (&focus_block);                             // between 'incoming' and 'outgoing' so they can be tagged for removal;

            while (focus_block != outgoing_block)
            {
              GCODE_UTIL_TAG_BLOCK (focus_block);

              gcode_get_circular_next (&focus_block);
            }

            break;                                                              // Once the cleanup is complete, time to move on to another 'spike';
          }

          if (focus_block != index_block)                                       // If we're still here, there was no intersection found,
          {
            gcode_get_circular_next (&focus_block);                             // so move 'focus' one step closer to 'index', if it's not there yet;
          }
          else                                                                  // If 'focus' reached 'index', nothing on this side intersects 'outgoing';
          {
            focus_block = incoming_block;                                       // So move 'focus' back out to the far 'incoming' end,

            gcode_get_circular_prev (&outgoing_block);                          // and bring 'outgoing' one step closer on the other side;
          }

          if (outgoing_block == index_block)                                    // If 'outgoing' regressed all the way across to the 'incoming' side,
            break;                                                              // nothing else remains to test - time to leave.
        }
      }
    }

    index_block = index_block->next;                                            // Move on to the next block to check for the next sharp joint;
  }

  gcode_util_remove_tagged_blocks (listhead);                                   // Everything unneeded has been tagged - proceed to remove all of it;
}

static void
gcode_sketch_add_up_path_length (gcode_block_t *listhead, gfloat_t *length)
{
  gcode_block_t *index_block;

  *length = 0;

  index_block = listhead;

  while (index_block)
  {
    (*length) += index_block->length (index_block);

    index_block = index_block->next;
  }
}

static void
gcode_sketch_flip_direction (gcode_block_t **listhead)
{
  gcode_block_t *index_block, *next_block;

  index_block = *listhead;

  if (!index_block)
    return;

  gcode_util_flip_direction (index_block);

  index_block = index_block->next;

  while (index_block)
  {
    next_block = index_block->next;

    gcode_util_flip_direction (index_block);

    gcode_place_block_before (*listhead, index_block);

    *listhead = index_block;

    index_block = next_block;
  }
}

void
gcode_sketch_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_sketch_t *sketch;
  gcode_block_t *extrusion_block;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_SKETCH, 0);

  (*block)->free = gcode_sketch_free;
  (*block)->save = gcode_sketch_save;
  (*block)->load = gcode_sketch_load;
  (*block)->make = gcode_sketch_make;
  (*block)->draw = gcode_sketch_draw;
  (*block)->aabb = gcode_sketch_aabb;
  (*block)->move = gcode_sketch_move;
  (*block)->spin = gcode_sketch_spin;
  (*block)->flip = gcode_sketch_flip;
  (*block)->scale = gcode_sketch_scale;
  (*block)->parse = gcode_sketch_parse;
  (*block)->clone = gcode_sketch_clone;

  (*block)->pdata = malloc (sizeof (gcode_sketch_t));

  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Sketch");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  sketch = (gcode_sketch_t *)(*block)->pdata;

  sketch->taper_offset[0] = 0.0;
  sketch->taper_offset[1] = 0.0;
  sketch->pocket = 0;
  sketch->zero_pass = 0;
  sketch->helical = 0;

  sketch->offset.side = 0.0;
  sketch->offset.tool = 0.0;
  sketch->offset.eval = 0.0;
  sketch->offset.rotation = 0.0;
  sketch->offset.origin[0] = 0.0;
  sketch->offset.origin[1] = 0.0;
  sketch->offset.z[0] = 0.0;
  sketch->offset.z[1] = 0.0;

  (*block)->offref = &sketch->offset;

  /* Create default extrusion */

  gcode_extrusion_init (&extrusion_block, gcode, *block);

  gcode_attach_as_extruder (*block, extrusion_block);
}

void
gcode_sketch_free (gcode_block_t **block)
{
  gcode_block_t *index_block, *tmp;

  /* Free the extrusion list */
  (*block)->extruder->free (&(*block)->extruder);

  /* Walk the list and free */
  index_block = (*block)->listhead;

  while (index_block)
  {
    tmp = index_block;
    index_block = index_block->next;
    tmp->free (&tmp);
  }

  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_sketch_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_sketch_t *sketch;
  uint32_t size, num, marker;
  uint8_t data;

  sketch = (gcode_sketch_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_SKETCH);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_2D_FLT (fh, GCODE_XML_ATTR_SKETCH_TAPER_OFFSET, sketch->taper_offset);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_SKETCH_POCKET, sketch->pocket);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_SKETCH_ZERO_PASS, sketch->zero_pass);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_SKETCH_HELICAL, sketch->helical);
    GCODE_WRITE_XML_OP_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    /**
     * In the XML branch the parent does NOT save even the common attributes
     * of its child blocks (as the binary branch does): saving some attributes
     * here and some (custom) attributes in the child would get rather messy
     */

    block->extruder->save (block->extruder, fh);

    index_block = block->listhead;

    while (index_block)
    {
      index_block->save (index_block, fh);

      index_block = index_block->next;
    }

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_SKETCH);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    /* SAVE EXTRUSION DATA */
    data = GCODE_BIN_DATA_SKETCH_EXTRUSION;
    size = 0;
    fwrite (&data, sizeof (uint8_t), 1, fh);

    /* Write block type */
    marker = ftell (fh);
    size = 0;
    fwrite (&size, sizeof (uint32_t), 1, fh);

    /* Write comment */
    GCODE_WRITE_BINARY_STR_DATA (fh, GCODE_BIN_DATA_BLOCK_COMMENT, block->extruder->comment);

    block->extruder->save (block->extruder, fh);

    size = ftell (fh) - marker - sizeof (uint32_t);
    fseek (fh, marker, SEEK_SET);
    fwrite (&size, sizeof (uint32_t), 1, fh);
    fseek (fh, marker + size + sizeof (uint32_t), SEEK_SET);

    num = 0;
    index_block = block->listhead;

    while (index_block)
    {
      num++;

      index_block = index_block->next;
    }

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_SKETCH_NUMBER, sizeof (uint32_t), &num);

    num = 0;
    index_block = block->listhead;

    while (index_block)
    {
      /* Write block type */
      fwrite (&index_block->type, sizeof (uint8_t), 1, fh);
      marker = ftell (fh);
      size = 0;
      fwrite (&size, sizeof (uint32_t), 1, fh);

      /* Write comment */
      GCODE_WRITE_BINARY_STR_DATA (fh, GCODE_BIN_DATA_BLOCK_COMMENT, index_block->comment);

      /* Write flags */
      GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BLOCK_FLAGS, sizeof (uint8_t), &index_block->flags);

      index_block->save (index_block, fh);

      size = ftell (fh) - marker - sizeof (uint32_t);
      fseek (fh, marker, SEEK_SET);
      fwrite (&size, sizeof (uint32_t), 1, fh);
      fseek (fh, marker + size + sizeof (uint32_t), SEEK_SET);

      index_block = index_block->next;
    }

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_SKETCH_TAPER_OFFSET, 2 * sizeof (gfloat_t), sketch->taper_offset);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_SKETCH_POCKET, sizeof (uint8_t), &sketch->pocket);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_SKETCH_ZERO_PASS, sizeof (uint8_t), &sketch->zero_pass);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_SKETCH_HELICAL, sizeof (uint8_t), &sketch->helical);
  }
}

void
gcode_sketch_load (gcode_block_t *block, FILE *fh)
{
  gcode_sketch_t *sketch;
  gcode_block_t *new_block;
  uint32_t bsize, dsize, start, num, i;
  uint8_t data, type;

  sketch = (gcode_sketch_t *)block->pdata;

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

      case GCODE_BIN_DATA_SKETCH_EXTRUSION:
        /* Rewind 4 bytes because the extrusion wants to read in its block size too. */
        fseek (fh, -4, SEEK_CUR);
        gcode_extrusion_load (block->extruder, fh);
        break;

      case GCODE_BIN_DATA_SKETCH_NUMBER:
        fread (&num, sizeof (uint32_t), 1, fh);

        for (i = 0; i < num; i++)
        {
          /* Read Data */
          fread (&type, sizeof (uint8_t), 1, fh);

          switch (type)
          {
            case GCODE_TYPE_ARC:
              gcode_arc_init (&new_block, block->gcode, block);
              break;

            case GCODE_TYPE_LINE:
              gcode_line_init (&new_block, block->gcode, block);
              break;

            default:
              break;
          }

          gcode_append_as_listtail (block, new_block);                          // Append 'new_block' to the end of 'block's list (as head if the list is NULL)

          new_block->load (new_block, fh);
        }
        break;

      case GCODE_BIN_DATA_SKETCH_TAPER_OFFSET:
        fread (&sketch->taper_offset, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_SKETCH_POCKET:
        fread (&sketch->pocket, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_SKETCH_ZERO_PASS:
        fread (&sketch->zero_pass, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_SKETCH_HELICAL:
        fread (&sketch->helical, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_sketch_make (gcode_block_t *block)
{
  gcode_sketch_t *sketch;
  gcode_extrusion_t *extrusion;
  gcode_tool_t *tool;
  gcode_block_t *sorted_listhead, *offset_listhead;
  gcode_block_t *start_block, *index_block, *index2_block;
  gcode_vec2d_t p0, p1, e0, e1, t;
  gfloat_t z, z0, z1, safe_z, touch_z;
  gfloat_t current_proffset, maximum_proffset;
  gfloat_t tool_radius, accum_length, path_length, path_drop, length_coef;
  int inside, closed, tapered, helical, initial;
  char string[256];

  GCODE_CLEAR (block);                                                          // Clean up the g-code string of this block to an empty string;

  if (!block->listhead)                                                         // If the list is empty, this will definitely be a quick pass...
    return;

  if (block->flags & GCODE_FLAGS_SUPPRESS)                                      // Same thing if the sketch is currently not supposed to be made;
    return;

  sketch = (gcode_sketch_t *)block->pdata;                                      // Get references to the custom data structs of the sketch and its extrusion;
  extrusion = (gcode_extrusion_t *)block->extruder->pdata;

  tool = gcode_tool_find (block);                                               // Find the tool that applies to this block;

  if (!tool)                                                                    // If there is none, this block will not get made - bail out;
    return;

  tool_radius = tool->diameter * 0.5;                                           // If a tool was found, obtain its radius;

  GCODE_NEWLINE (block);                                                        // Print some generic info and newlines into the g-code string;

  sprintf (string, "SKETCH: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  /**
   * WARNING: Code is being duplicated into the sketch.  This is wasteful,
   * but the amount of waste memory as of right now is negligible.
   */

  tapered = gcode_extrusion_taper_exists (block->extruder);                     // Find out whether the extrusion is strictly vertical or not;

  block->extruder->ends (block->extruder, p0, p1, GCODE_GET);                   // Find the start and end depth of the extrusion curve;

  if (p0[1] > p1[1])                                                            // If the first depth is above the second, store the z-values as they are;
  {
    z0 = p0[1];
    z1 = p1[1];
  }
  else                                                                          // If the first depth is below the second, swap the z-values before storing;
  {
    z0 = p1[1];
    z1 = p0[1];
  }

  index_block = block->listhead;                                                // Start with the first child in the list;

  gcode_util_get_sublist_snapshot (&sorted_listhead, index_block, NULL);        // First off, we need a working snapshot of the entire list ('sorted_listhead')
  gcode_util_remove_null_sections (&sorted_listhead);                           // We also need to remove any zero-sized features that could screw up the math;
  gcode_util_merge_list_fragments (&sorted_listhead);                           // But why is it called 'sorted' when it's a straight copy? Oh, that's why...

  index_block = sorted_listhead;                                                // Start crawling along the sorted list;

  while (index_block)                                                           // Keep looping as long as the chain lasts, one sub-chain per loop;
  {
    start_block = index_block;                                                  // Save a reference to the first block in the current sub-chain;

    while (index_block->next)                                                   // As long as there is a next block, crawl along the sub-chain;
    {
      index_block->ends (index_block, t, e0, GCODE_GET);                        // Obtain the endpoints of the current block and the next one;
      index_block->next->ends (index_block->next, e1, t, GCODE_GET);

      if (GCODE_MATH_2D_MANHATTAN (e0, e1) > GCODE_TOLERANCE)                   // If the Manhattan distance from the ending point of the current block
        break;                                                                  // to the starting point of the next is significant, break out of the loop;

      index_block = index_block->next;                                          // If the endpoints did meet, keep following the sub-chain;
    }

    start_block->ends (start_block, e0, t, GCODE_GET);                          // Obtain the starting point of the saved first block,
    index_block->ends (index_block, t, e1, GCODE_GET);                          // and the ending point of the last contiguous one;

    closed = (GCODE_MATH_2D_MANHATTAN (e0, e1) < GCODE_TOLERANCE) ? 1 : 0;      // If they are practically identical, the sub-chain is 'closed';

    helical = (closed && sketch->helical && !tapered) ? 1 : 0;                  // Once 'closed or not' is known, 'helical or not' can be decided;

    sketch->offset.tool = tool_radius;                                          // Making very much depends on the tool size - set it up;

    /**
     * Determine if the right side of the first line is inside or outside the sketch.
     * Use this to tell whether the offset by the extrusion sketch should be right
     * or left of the primitive.
     */

    if (closed)
      sketch->offset.side = gcode_sketch_inside (start_block, index_block);
    else
      sketch->offset.side = 0;

    switch (extrusion->cut_side)
    {
      case GCODE_EXTRUSION_OUTSIDE:
        sketch->offset.side *= 1.0;
        break;

      case GCODE_EXTRUSION_INSIDE:
        sketch->offset.side *= -1.0;
        break;

      case GCODE_EXTRUSION_ALONG:
        sketch->offset.side *= 0.0;
        sketch->offset.tool *= 0.0;
        break;
    }

    initial = 1;                                                                // For the first pass (whether it is a 'zero pass' or not) this stays true;

    safe_z = block->gcode->ztraverse;                                           // This is just a short-hand for the traverse z...

    touch_z = block->gcode->material_origin[2];                                 // Track the depth the material begins at (gets lower after every pass);

    if (sketch->zero_pass || sketch->helical)                                   // Start the first pass at z0 if zero_pass or helical is specified;
      z = z0;
    else if (z0 - z1 > extrusion->resolution)                                   // If not, start one step deeper if the total depth is larger than one step;
      z = z0 - extrusion->resolution;
    else                                                                        // If even a single step would be too deep, start the pass at z1 instead;
      z = z1;

    GCODE_RETRACT (block, safe_z);                                              // Retract - should already be retracted, but here for safety reasons;

    while (z >= z1)                                                             // Loop within the current sub-chain, creating one pass depth per loop;
    {
      GCODE_NEWLINE (block);

      gsprintf (string, block->gcode->decimals, "Pass at depth: %z", z);
      GCODE_COMMENT (block, string);

      GCODE_NEWLINE (block);

      sketch->offset.origin[0] = block->offset->origin[0];                      // Start by inheriting the offset of the parent; considering the taper's effect
      sketch->offset.origin[1] = block->offset->origin[1];                      // changes with the depth, we need to re-apply this on every z-pass...
      sketch->offset.rotation = block->offset->rotation;

      sketch->offset.origin[0] += sketch->taper_offset[0] * (z0 - z) / (z0 - z1);       // Anyway, the rest of the offset must be applied: the taper offset...
      sketch->offset.origin[1] += sketch->taper_offset[1] * (z0 - z) / (z0 - z1);

      gcode_extrusion_evaluate_offset (block->extruder, z, &current_proffset);  // ...and the extrusion profile offset calculated for the current z-depth;

      sketch->offset.eval = current_proffset;

      gcode_util_get_sublist_snapshot (&offset_listhead, start_block, index_block);     // Another working snapshot is needed, so we can preserve 'sorted_list';
      gcode_util_convert_to_no_offset (offset_listhead);                        // Recalculate that snapshot with offsets included & link it to a zero offset;
      gcode_util_tag_null_size_blocks (offset_listhead);                        // Tag but do not remove anything that BECAME zero-sized by applying the offset;

      gcode_sketch_trim_intersections (offset_listhead, closed);                // Find and trim intersecting primitives back to the intersection point;
      gcode_sketch_insert_transitions (offset_listhead, closed);                // Add transition arcs wherever connected endpoints were pulled apart;

      gcode_util_remove_tagged_blocks (&offset_listhead);                       // Remove the tagged blocks - they are no longer needed, the contour is done;

      gcode_sketch_check_sharp_points (&offset_listhead, closed);

      /**
       * POCKETING:
       *   Implies that sketch section is closed.
       *   Perform inside pocketing if explicitly set or automatically do:
       *   - Outside pocket if extrusion is outward
       *   - Inside pocket if extrusion is inward (same as explicitly set)
       */

      if (closed && (sketch->pocket || tapered))
      {
        switch (extrusion->cut_side)
        {
          case GCODE_EXTRUSION_INSIDE:                                          // Inward Taper;
          {
            gcode_pocket_t pocket;

            gcode_pocket_init (&pocket, block, tool);                           // Create a pocket for 'offset_listhead';
            gcode_pocket_prep (&pocket, offset_listhead, NULL);                 // Create a raster of paths based on the contour;
            gcode_pocket_make (&pocket, z, touch_z);                            // Create the g-code from the pocket's path list;
            gcode_pocket_free (&pocket);                                        // Dispose of the no longer needed pocket;

            break;
          }

          case GCODE_EXTRUSION_OUTSIDE:                                         // Outward Taper;
          {
            gcode_pocket_t inner_pocket, outer_pocket;
            gcode_block_t *inner_offset_listhead, *outer_offset_listhead;

            /**
             *  Pocketing applied automatically to the difference between the
             *  outer[final z value] offset and inner[current z value] offset.
             */

            inner_offset_listhead = offset_listhead;                            // The main contour becomes the inner island;

            sketch->offset.origin[0] = block->offset->origin[0];                // We need to construct a whole new contour (with the offset at depth 'z1'),
            sketch->offset.origin[1] = block->offset->origin[1];                // so have to redo everything we did to get the first list (for depth 'z');
            sketch->offset.rotation = block->offset->rotation;                  // That means recalculating the offset origin, rotation and eval for 'z1';

            sketch->offset.origin[0] += sketch->taper_offset[0];                // Anyway, the reason we can freely mess up the offset of the sketch is that
            sketch->offset.origin[1] += sketch->taper_offset[1];                // it is already applied for this round and will be recalculated in the next;

            gcode_extrusion_evaluate_offset (block->extruder, z1, &maximum_proffset);

            sketch->offset.eval = maximum_proffset;

            if (fabs (maximum_proffset - current_proffset) > GCODE_PRECISION)   // If the current profile offset is equal to the largest profile offset,
            {                                                                   // there is no need at all for the outer pass, it matches the inner one;
              gcode_util_get_sublist_snapshot (&outer_offset_listhead, start_block, index_block);       // Once the offset is ready, create another working snapshot;
              gcode_util_convert_to_no_offset (outer_offset_listhead);          // Recalculate the snapshot with the new 'z1' offsets included & a zero offset;
              gcode_util_tag_null_size_blocks (outer_offset_listhead);          // Tag but do not remove anything that BECAME zero-sized by applying the offset;

              gcode_sketch_trim_intersections (outer_offset_listhead, closed);  // Find and trim intersecting primitives back to the intersection point;
              gcode_sketch_insert_transitions (outer_offset_listhead, closed);  // Add transition arcs wherever connected endpoints were pulled apart;

              gcode_util_remove_tagged_blocks (&outer_offset_listhead);         // Remove the tagged blocks - they are no longer needed, the contour is done;

              gcode_sketch_check_sharp_points (&outer_offset_listhead, closed);

              if (fabs (maximum_proffset - current_proffset) > tool->diameter)  // The thing is, if the current profile offset is less than a tool diameter
              {                                                                 // away from the maximum profile offset, there is no need to pocket at all;
                gcode_pocket_init (&inner_pocket, block, tool);                 // Create two pockets, one for the inner contour, one for the outer;
                gcode_pocket_init (&outer_pocket, block, tool);

                gcode_pocket_prep (&inner_pocket, inner_offset_listhead, NULL); // Create the inner pocket from the current 'z' depth contour / list;
                gcode_pocket_prep (&outer_pocket, outer_offset_listhead, NULL); // Create the outer pocket from the final 'z1' depth contour / list;

                gcode_pocket_subtract (&outer_pocket, &inner_pocket);           // Subtract the inner pocket from the outer one,
                gcode_pocket_make (&outer_pocket, z, touch_z);                  // and create the g-code resulting from what remains;

                gcode_pocket_free (&inner_pocket);                              // The two pockets are no longer needed and can be disposed of;
                gcode_pocket_free (&outer_pocket);
              }

              gcode_sketch_flip_direction (&outer_offset_listhead);             // Flip the outer contour in order to match the milling mode of the inner one;

              outer_offset_listhead->ends (outer_offset_listhead, e0, e1, GCODE_GET_WITH_OFFSET);

              GCODE_NEWLINE (block);

              GCODE_COMMENT (block, "Secondary Contour Milling Phase");

              GCODE_NEWLINE (block);

              GCODE_MOVE_TO (block, e0[0], e0[1], z, safe_z, touch_z, tool, "start of contour");

              index2_block = outer_offset_listhead;

              while (index2_block)
              {
                index2_block->offset->z[0] = z;
                index2_block->offset->z[1] = z;

                index2_block->make (index2_block);
                GCODE_APPEND (block, index2_block->code);

                index2_block = index2_block->next;
              }

              free (outer_offset_listhead->offset);                             // The specially created zero-offset of the list is no longer needed;
              gcode_list_free (&outer_offset_listhead);                         // The pocket is done, we can get rid of the 'outer' / 'z1' list as well;
            }

            break;
          }
        }
      }

      /**
       * Pocketing is complete, get in position for the contour pass
       */

      offset_listhead->ends (offset_listhead, e0, e1, GCODE_GET_WITH_OFFSET);   // Find again the starting point of the first block;

      GCODE_NEWLINE (block);

      GCODE_COMMENT (block, "Primary Contour Milling Phase");

      GCODE_NEWLINE (block);

      GCODE_MOVE_TO (block, e0[0], e0[1], z, safe_z, touch_z, tool, "start of contour");        // Go there (outputs nothing if already there);

      /**
       * Generate G-Code for each block primitive in a contour pass
       */

      gcode_sketch_add_up_path_length (offset_listhead, &path_length);          // Calculate the full path length for use in helical z-depth calculations;

      accum_length = 0.0;                                                       // Start logging distance along the path for helical z-depth calculations;
      index2_block = offset_listhead;                                           // Start drawing the processed list, starting with 'offset_listhead';

      while (index2_block)                                                      // Loop around and draw the processed list (the current sub-chain);
      {
        if (helical && (z - z1 > GCODE_PRECISION))                              // If helical milling is active, z-depth is recalculated per block;
        {
          if (z - z1 < extrusion->resolution)                                   // See what we have to play with: a full 'resolution' depth step or less?
            path_drop = z - z1;
          else
            path_drop = extrusion->resolution;

          length_coef = accum_length / path_length;                             // Calculate the fraction of the total path length that 'z[0]' is at;
          index2_block->offset->z[0] = z - path_drop * length_coef;             // The same fraction of the total path drop is how far 'z[0]' is below 'z';

          accum_length += index2_block->length (index2_block);                  // Add the length of the current block to the current distance along the path;

          length_coef = accum_length / path_length;                             // Calculate the fraction of the total path length that 'z[1]' is at;
          index2_block->offset->z[1] = z - path_drop * length_coef;             // The same fraction of the total path drop is how far 'z[1]' is below 'z';
        }
        else                                                                    // Oh, this is the trivial case - if helical is not active, everything happens at 'z';
        {
          index2_block->offset->z[0] = z;
          index2_block->offset->z[1] = z;
        }

        index2_block->make (index2_block);                                      // FINALLY! Just make that darned block and be done with it...
        GCODE_APPEND (block, index2_block->code);

        index2_block = index2_block->next;                                      // ...well, not before we do the same thing for each of them.
      }

      free (offset_listhead->offset);                                           // The specially created zero-offset is no longer needed;
      gcode_list_free (&offset_listhead);                                       // This sub-chain has been milled - get rid of the offset list;

      initial = 0;                                                              // Further passes are not the 'first pass' any more;
      touch_z = z;                                                              // The current z depth is now the new boundary between air and material;

      if (z - z1 > extrusion->resolution)                                       // Go one level deeper if the remaining depth is larger than one depth step;
        z = z - extrusion->resolution;
      else if (z - z1 > GCODE_PRECISION)                                        // If it's less but still a significant amount, go to the final z1 instead;
        z = z1;
      else                                                                      // If the depth of the last pass is already indistinguishable from z1, quit;
        break;
    }

    index_block = index_block->next;                                            // Move on to the next sub-chain: continue with the first 'unconnected' block;
  }

  GCODE_RETRACT (block, safe_z);                                                // The entire sketch has been milled - raise to traverse height;

  gcode_list_free (&sorted_listhead);                                           // Once the sketch is done, get rid of the sorted list;

  sketch->offset.side = 0.0;
  sketch->offset.tool = 0.0;
  sketch->offset.eval = 0.0;
}

/**
 * Draw the contents of the sketch either as several contour lines (one for each
 * milling pass as determined by the extrusion resolution) or as a single curve
 * at top level (if an element of the sketch is currently selected for editing);
 * NOTE: the term 'single curve' is used loosely here - there will be as many
 * distinct curves per pass as there are disjointed sections in the sketch...
 * NOTE: it would be nice to check for existence of all pertinent functions like
 * 'ends', 'eval' etc. on the full list, the extrusion AND its list but for now
 * we're just going to assert all blocks involved are of a type that has those;
 */

void
gcode_sketch_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_sketch_t *sketch;
  gcode_extrusion_t *extrusion;
  gcode_block_t *sorted_listhead;
  gcode_block_t *start_block, *index_block, *index2_block;
  gcode_vec2d_t p0, p1, e0, e1, t;
  gfloat_t z, z0, z1;
  gfloat_t accum_length, path_length, path_drop, length_coef;
  int edited, inside, closed, tapered;

  if (!block->listhead)                                                         // If the list is empty, this will definitely be a quick pass...
    return;

  if (block->flags & GCODE_FLAGS_SUPPRESS)                                      // Same thing if the sketch is currently not supposed to be drawn;
    return;

  sketch = (gcode_sketch_t *)block->pdata;                                      // Get references to the custom data structs of the sketch and its extrusion;
  extrusion = (gcode_extrusion_t *)block->extruder->pdata;

  tapered = gcode_extrusion_taper_exists (block->extruder);                     // Find out whether the extrusion is strictly vertical or not;

  block->extruder->ends (block->extruder, p0, p1, GCODE_GET);                   // Find the start and end depth of the extrusion curve;

  if (p0[1] > p1[1])                                                            // If the first depth is above the second, store the z-values as they are;
  {
    z0 = p0[1];
    z1 = p1[1];
  }
  else                                                                          // If the first depth is below the second, swap the z-values before storing;
  {
    z0 = p1[1];
    z1 = p0[1];
  }

  index_block = selected ? selected->parent : NULL;                             // If this sketch is the parent of the selected block, we're in 'edit' mode;

  edited = (index_block == block) ? TRUE : FALSE;                               // We need that information later, but by then 'index_block' will be in use.

  index_block = block->listhead;                                                // Start with the first child in the list;

  gcode_util_get_sublist_snapshot (&sorted_listhead, index_block, NULL);        // First off, we need a working snapshot of the entire list ('sorted_listhead')
  gcode_util_remove_null_sections (&sorted_listhead);                           // We also need to remove any zero-sized features that could screw up the math;
  gcode_util_merge_list_fragments (&sorted_listhead);                           // But why is it called 'sorted' when it's a straight copy? Oh, that's why...

  index_block = sorted_listhead;                                                // Start crawling along the sorted list;

  while (index_block)                                                           // Keep looping as long as the chain lasts, one sub-chain per loop;
  {
    start_block = index_block;                                                  // Save a reference to the first block in the current sub-chain;

    while (index_block->next)                                                   // As long as there is a next block, crawl along the sub-chain;
    {
      index_block->ends (index_block, t, e0, GCODE_GET);                        // Obtain the endpoints of the current block and the next one;
      index_block->next->ends (index_block->next, e1, t, GCODE_GET);

      if (GCODE_MATH_2D_MANHATTAN (e0, e1) > GCODE_TOLERANCE)                   // If the Manhattan distance from the ending point of the current block
        break;                                                                  // to the starting point of the next is significant, break out of the loop;

      index_block = index_block->next;                                          // If the endpoints did meet, keep following the sub-chain;
    }

    start_block->ends (start_block, e0, t, GCODE_GET);                          // Obtain the starting point of the saved first block,
    index_block->ends (index_block, t, e1, GCODE_GET);                          // and the ending point of the last contiguous one;

    closed = (GCODE_MATH_2D_MANHATTAN (e0, e1) < GCODE_TOLERANCE) ? 1 : 0;      // If they are practically identical, the sub-chain is 'closed';

    if (edited)                                                                 // THE EASY WAY - drawing the sketch with its inherited offset, nothing more;
    {
      gcode_block_t *sliced_listhead;                                           // Unaltered snapshot of the current contiguous slice of 'sorted_list';

      sketch->offset.origin[0] = block->offset->origin[0];                      // Start by inheriting the offset of the parent; and for now, that's all we do.
      sketch->offset.origin[1] = block->offset->origin[1];                      // The rest of the offset caused by the extrusion profile should NOT be applied
      sketch->offset.rotation = block->offset->rotation;                        // to the top-layer 'edit' view: IMHO it should reflect what the numbers imply.

      sketch->offset.side = 0.0;                                                // As this view is not affected by the extrusion profile, it needs no side;
      sketch->offset.tool = 0.0;                                                // The drawing operation is not influenced by tool size, so set it to zero;
      sketch->offset.eval = 0.0;                                                // The 'edit' view is not affected by the extrusion profile, set it to zero;

      sketch->offset.z[0] = z0;                                                 // As z-offset arbitrarily use the extrusion top level (we could just use "0");
      sketch->offset.z[1] = z0;

      gcode_util_get_sublist_snapshot (&sliced_listhead, start_block, index_block);     // Another working snapshot is needed, so we can preserve 'sorted_list';

      index2_block = sliced_listhead;                                           // With no extrusion, take the ordered list and start with the first child;

      while (index2_block)                                                      // Loop around and draw the ordered list section (the current sub-chain);
      {
        index2_block->draw (index2_block, selected);                            // Call the 'draw' method of each block ('draw' applies offset automatically);

        index2_block = index2_block->next;                                      // Keep crawling along the sub-chain;
      }

      gcode_list_free (&sliced_listhead);                                       // This sub-chain has been drawn - get rid of the list section snapshot;
    }
    else                                                                        // THE HARD WAY - extrude, offset, intersect, transition: tiresome stuff.
    {
      gcode_block_t *offset_listhead;                                           // Offset snapshot of the current contiguous slice of 'sorted_list';

      sketch->offset.tool = 0.0;                                                // Drawing is not influenced by tool size - set it to zero;

      /**
       * Determine if the right side of the first line is inside or outside the
       * sketch. Use this to decide whether the offset caused by the extrusion
       * should be right or left of the primitive.
       */

      if (closed)
        sketch->offset.side = gcode_sketch_inside (start_block, index_block);
      else
        sketch->offset.side = 0;

      switch (extrusion->cut_side)
      {
        case GCODE_EXTRUSION_OUTSIDE:
          sketch->offset.side *= 1.0;
          break;

        case GCODE_EXTRUSION_INSIDE:
          sketch->offset.side *= -1.0;
          break;

        case GCODE_EXTRUSION_ALONG:
          sketch->offset.side *= 0.0;
          break;
      }

      if (sketch->zero_pass || sketch->helical)                                 // Start the first pass at z0 if zero_pass or helical is specified;
        z = z0;
      else if (z0 - z1 > extrusion->resolution)                                 // If not, start one step deeper if the total depth is larger than one step;
        z = z0 - extrusion->resolution;
      else                                                                      // If even a single step would be too deep, start the pass at z1 instead;
        z = z1;

      while (z >= z1)                                                           // Loop within the current sub-chain, creating one pass depth per loop;
      {
        sketch->offset.origin[0] = block->offset->origin[0];                    // Start by inheriting the offset of the parent; considering the taper's effect
        sketch->offset.origin[1] = block->offset->origin[1];                    // changes with the depth, we need to re-apply this on every z-pass...
        sketch->offset.rotation = block->offset->rotation;

        sketch->offset.origin[0] += sketch->taper_offset[0] * (z0 - z) / (z0 - z1);     // Anyway, the rest of the offset must be applied: the taper offset...
        sketch->offset.origin[1] += sketch->taper_offset[1] * (z0 - z) / (z0 - z1);

        gcode_extrusion_evaluate_offset (block->extruder, z, &sketch->offset.eval);     // ...and the extrusion profile offset calculated for the current z-depth;

        gcode_util_get_sublist_snapshot (&offset_listhead, start_block, index_block);   // Another working snapshot is needed, so we can preserve 'sorted_list';
        gcode_util_convert_to_no_offset (offset_listhead);                      // Recalculate that snapshot with offsets included & link it to a zero offset;
        gcode_util_tag_null_size_blocks (offset_listhead);                      // Tag but do not remove anything that BECAME zero-sized by applying the offset;

        gcode_sketch_trim_intersections (offset_listhead, closed);              // Find and trim intersecting primitives back to the intersection point;
        gcode_sketch_insert_transitions (offset_listhead, closed);              // Add transition arcs wherever connected endpoints were pulled apart;

        gcode_util_remove_tagged_blocks (&offset_listhead);                     // Remove the tagged blocks - they are no longer needed, the contour is done;

        gcode_sketch_check_sharp_points (&offset_listhead, closed);
        gcode_sketch_add_up_path_length (offset_listhead, &path_length);        // Calculate the full path length for use in helical z-depth calculations;

        accum_length = 0.0;                                                     // Start logging distance along the path for helical z-depth calculations;
        index2_block = offset_listhead;                                         // Start drawing the processed list, starting with 'offset_listhead';

        while (index2_block)                                                    // Loop around and draw the processed list (the current sub-chain);
        {
          if (closed && sketch->helical && !tapered && (z - z1 > GCODE_PRECISION))      // If helical milling is active, z-depth is recalculated per block;
          {
            if (z - z1 < extrusion->resolution)                                 // See what we have to play with: a full 'resolution' depth step or less?
              path_drop = z - z1;
            else
              path_drop = extrusion->resolution;

            length_coef = accum_length / path_length;                           // Calculate the fraction of the total path length that 'z[0]' is at;
            index2_block->offset->z[0] = z - path_drop * length_coef;           // The same fraction of the total path drop is how far 'z[0]' is below 'z';

            accum_length += index2_block->length (index2_block);                // Add the length of the current block to the current distance along the path;

            length_coef = accum_length / path_length;                           // Calculate the fraction of the total path length that 'z[1]' is at;
            index2_block->offset->z[1] = z - path_drop * length_coef;           // The same fraction of the total path drop is how far 'z[1]' is below 'z';
          }
          else                                                                  // Oh, this is the trivial case - if helical is not active, everything happens at 'z';
          {
            index2_block->offset->z[0] = z;
            index2_block->offset->z[1] = z;
          }

          index2_block->draw (index2_block, selected);                          // FINALLY! Just draw that darned block and be done with it...

          index2_block = index2_block->next;                                    // ...well, not before we do the same thing for each of them.
        }

        free (offset_listhead->offset);                                         // The specially created zero-offset is no longer needed;
        gcode_list_free (&offset_listhead);                                     // This sub-chain has been drawn - get rid of the offset list;

        if (z - z1 > extrusion->resolution)                                     // Go one level deeper if the remaining depth is larger than one depth step;
          z = z - extrusion->resolution;
        else if (z - z1 > GCODE_PRECISION)                                      // If it's less but still a significant amount, go to the final z1 instead;
          z = z1;
        else                                                                    // If the depth of the last pass is already indistinguishable from z1, quit;
          break;
      }
    }

    index_block = index_block->next;                                            // Move on to the next sub-chain: continue with the first 'unconnected' block;
  }

  gcode_list_free (&sorted_listhead);                                           // The entire sketch has been drawn - get rid of the sorted list;

  sketch->offset.side = 0.0;                                                    // Not of any importance strictly speaking (anywhere these actually matter they
  sketch->offset.tool = 0.0;                                                    // should be re-initialized appropriately anyway), but hey - let's play nice...
  sketch->offset.eval = 0.0;
#endif
}

/**
 * Construct the axis-aligned bounding box of all the members of the sketch;
 * NOTE: this can and does return an "imposible" or "inside-out" bounding box
 * which has its minimum larger than its maximum as a sign of failure to pick
 * up any member with a valid bounding box, if either the list is empty or the
 * members all lack an "aabb" function - THIS SHOULD BE TESTED FOR ON RETURN!
 */

void
gcode_sketch_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max)
{
  gcode_block_t *index_block;
  gcode_vec2d_t tmin, tmax;

  index_block = block->listhead;

  min[0] = min[1] = 1;                                                          // Never cross the streams, you say...? Oh well, too late...
  max[0] = max[1] = 0;                                                          // Callers should test for an inside-out aabb being returned;

  while (index_block)
  {
    if (!index_block->aabb)                                                     // If the block has no bounds function, don't try to call it;
    {
      index_block = index_block->next;
      continue;
    }

    index_block->aabb (index_block, tmin, tmax);

    if ((tmin[0] > tmax[0]) || (tmin[1] > tmax[1]))                             // If the block returned an inside-out box, discard the box;
    {
      index_block = index_block->next;
      continue;
    }

    if ((min[0] > max[0]) || (min[1] > max[1]))                                 // If bounds were inside-out (unset), accept the box directly;
    {
      min[0] = tmin[0];
      min[1] = tmin[1];
      max[0] = tmax[0];
      max[1] = tmax[1];
    }
    else                                                                        // Nothing ever is too simple to screw up, eh...?
    {
      if (tmin[0] < min[0])
        min[0] = tmin[0];

      if (tmax[0] > max[0])
        max[0] = tmax[0];

      if (tmin[1] < min[1])
        min[1] = tmin[1];

      if (tmax[1] > max[1])
        max[1] = tmax[1];
    }

    index_block = index_block->next;
  }
}

void
gcode_sketch_move (gcode_block_t *block, gcode_vec2d_t delta)
{
  gcode_block_t *index_block;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->move)
      index_block->move (index_block, delta);

    index_block = index_block->next;
  }
}

void
gcode_sketch_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_block_t *index_block;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->spin)
      index_block->spin (index_block, datum, angle);

    index_block = index_block->next;
  }
}

void
gcode_sketch_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_block_t *index_block;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->flip)
      index_block->flip (index_block, datum, angle);

    index_block = index_block->next;
  }
}

void
gcode_sketch_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_block_t *index_block;
  gcode_sketch_t *sketch;

  sketch = (gcode_sketch_t *)block->pdata;

  sketch->taper_offset[0] *= scale;
  sketch->taper_offset[1] *= scale;

  block->extruder->scale (block->extruder, scale);

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->scale)
      index_block->scale (index_block, scale);

    index_block = index_block->next;
  }
}

void
gcode_sketch_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_sketch_t *sketch;

  sketch = (gcode_sketch_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_SKETCH_TAPER_OFFSET) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_FLT (xyz, value))
        for (int j = 0; j < 2; j++)
          sketch->taper_offset[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_SKETCH_POCKET) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        sketch->pocket = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_SKETCH_ZERO_PASS) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        sketch->zero_pass = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_SKETCH_HELICAL) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        sketch->helical = m;
    }
  }
}

void
gcode_sketch_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_sketch_t *sketch, *model_sketch;
  gcode_block_t *index_block, *new_block;

  model_sketch = (gcode_sketch_t *)model->pdata;

  gcode_sketch_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  sketch = (gcode_sketch_t *)(*block)->pdata;

  sketch->taper_offset[0] = model_sketch->taper_offset[0];
  sketch->taper_offset[1] = model_sketch->taper_offset[1];
  sketch->offset = model_sketch->offset;
  sketch->pocket = model_sketch->pocket;
  sketch->zero_pass = model_sketch->zero_pass;

  model->extruder->clone (&new_block, gcode, model->extruder);                  // Acquire a clone of the model's extruder, referenced as 'new_block'

  gcode_attach_as_extruder (*block, new_block);                                 // Attach the brand new extrusion clone as the extruder of this block

  index_block = model->listhead;

  while (index_block)
  {
    index_block->clone (&new_block, gcode, index_block);

    gcode_append_as_listtail (*block, new_block);                               // Append 'new_block' to the end of 'block's list (as head if the list is NULL)

    index_block = index_block->next;
  }
}

/**
 * Multiply the elements of a sketch a number of times, each time applying an
 * incremental rotation and translation offset, creating a regularly repeating
 * pattern from the original elements of the block.
 */

void
gcode_sketch_pattern (gcode_block_t *block, uint32_t count, gcode_vec2d_t delta, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_block_t *index_block, *final_block, *new_block, *last_block;
  gfloat_t inc_rotation, inc_translate_x, inc_translate_y;
  uint32_t i;

  if (block->listhead)                                                          // No point to stick around if the list is empty.
  {
    final_block = block->listhead;                                              // If we're still here, the list is NOT empty: start from the first block.

    while (final_block->next)                                                   // Crawl along the list until 'final_block' really points to the last block.
      final_block = final_block->next;

    last_block = final_block;                                                   // Point 'last_block' as well to the last block in the list.

    for (i = 1; i < count; i++)                                                 // If 'count' equals 0 or 1, this will do nothing at all - as it should.
    {
      inc_rotation = ((float)i) * angle;                                        // Calculate roto-translation factors for the current iteration
      inc_translate_x = ((float)i) * delta[0];
      inc_translate_y = ((float)i) * delta[1];

      index_block = block->listhead;                                            // Each time, start with the first block in the list (which we know must exist)

      for (;;)                                                                  // Y'all keep coming back until I say otherwise, y'hear?
      {
        switch (index_block->type)                                              // Depending on the type of the current block, create a new one;
        {
          case GCODE_TYPE_ARC:
          {
            gcode_arc_t *arc, *new_arc;
            gcode_vec2d_t xform_pt, pt;

            arc = (gcode_arc_t *)index_block->pdata;                            // Acquire a reference to the data of the current block;

            gcode_arc_init (&new_block, block->gcode, block);                   // Create the new block,
            new_arc = (gcode_arc_t *)new_block->pdata;                          // and acquire a reference to its data as well.

            /* Rotate and Translate */
            pt[0] = arc->p[0] - datum[0];
            pt[1] = arc->p[1] - datum[1];
            GCODE_MATH_ROTATE (xform_pt, pt, inc_rotation);
            new_arc->p[0] = xform_pt[0] + datum[0] + inc_translate_x;
            new_arc->p[1] = xform_pt[1] + datum[1] + inc_translate_y;
            new_arc->sweep_angle = arc->sweep_angle;
            new_arc->radius = arc->radius;
            new_arc->start_angle = arc->start_angle + inc_rotation;
            GCODE_MATH_WRAP_TO_360_DEGREES (new_arc->start_angle);

            break;
          }

          case GCODE_TYPE_LINE:
          {
            gcode_line_t *line, *new_line;
            gcode_vec2d_t xform_pt, pt;

            line = (gcode_line_t *)index_block->pdata;                          // Acquire a reference to the data of the current block;

            gcode_line_init (&new_block, block->gcode, block);                  // Create the new block,
            new_line = (gcode_line_t *)new_block->pdata;                        // and acquire a reference to its data as well.

            /* Rotate and Translate */
            pt[0] = line->p0[0] - datum[0];
            pt[1] = line->p0[1] - datum[1];
            GCODE_MATH_ROTATE (xform_pt, pt, inc_rotation);
            new_line->p0[0] = xform_pt[0] + datum[0] + inc_translate_x;
            new_line->p0[1] = xform_pt[1] + datum[1] + inc_translate_y;

            pt[0] = line->p1[0] - datum[0];
            pt[1] = line->p1[1] - datum[1];
            GCODE_MATH_ROTATE (xform_pt, pt, inc_rotation);
            new_line->p1[0] = xform_pt[0] + datum[0] + inc_translate_x;
            new_line->p1[1] = xform_pt[1] + datum[1] + inc_translate_y;

            break;
          }
        }

        strcpy (new_block->comment, index_block->comment);                      // Copy the comment string of the current block to the new one;

        gcode_insert_after_block (last_block, new_block);                       // Attach the new block after the current last block in the list,

        last_block = new_block;                                                 // and designate it as the new last block in the list;

        if (index_block != final_block)                                         // If the current block is not the one that was the last in the original list
          index_block = index_block->next;                                      // (as referenced by 'final_block'), keep crawling along.
        else                                                                    // If the current block is the one that used to be the last one originally,
          break;                                                                // Y'all may now go! Y'all be coming right back for the next iteration anyways.
      }
    }
  }
}

/**
 * Return '1' if the children of 'block' form one or more closed contours or '0'
 * if they do not. The elements of the sketch do not need to form a contiguous
 * chain of touching endpoints, non-contiguous sub-chains just need to end where
 * they started (all sub-curves must be closed) to qualify the sketch 'closed';
 * More significantly, the individual sections of the contour don't even need to
 * be in the right (strictly contiguous) order - they just need to be orderable
 * into a contour that would normally qualify as 'closed';
 * NOTE: the speculative nature of this routine implies an ordering attempt is
 * made on un-ordered sketches that can potentially take a significant amount of
 * time for sketches with a very large number of blocks in a very poor order.
 */

int
gcode_sketch_is_closed (gcode_block_t *block)
{
  gcode_block_t *working_listhead;
  int closed;

  gcode_util_get_sublist_snapshot (&working_listhead, block->listhead, NULL);   // Get a working snapshot of the list - we can't try sorting the original;
  gcode_util_remove_null_sections (&working_listhead);                          // We also need to remove any zero-sized features that could screw up the math;

  closed = gcode_util_merge_list_fragments (&working_listhead);                 // Finally, see if the list can be arranged into a closed contour in any way;

  return (closed);                                                              // If so, let us know... ;)
}

/**
 * Return '1' if the children of 'block' form one or more closed contours or '0'
 * if they do not. The elements of the sketch do not need to form a contiguous
 * chain of touching endpoints, non-contiguous sub-chains just need to end where
 * they started (all sub-curves must be closed) to qualify the sketch 'closed';
 * The individual sections need to follow each other strictly though - blocks in
 * the same sub-chain each need the start exactly where the previous one ended.
 */

int
gcode_sketch_is_joined (gcode_block_t *block)
{
  gcode_block_t *start_block, *index_block;
  gcode_vec2d_t e0, e1, t;
  int closed = 1;                                                               // Start out assuming the sketch is 'closed';

  index_block = block->listhead;                                                // Start with the first child in the list;

  while (index_block)                                                           // Keep looping as long as the chain lasts, one sub-chain per loop;
  {
    start_block = index_block;                                                  // Save a reference to the first block in the current sub-chain;

    while (index_block->next)                                                   // As long as there is a next block, crawl along the sub-chain;
    {
      index_block->ends (index_block, t, e0, GCODE_GET);                        // Obtain the endpoints of the current block and the next one;
      index_block->next->ends (index_block->next, e1, t, GCODE_GET);

      if (GCODE_MATH_2D_MANHATTAN (e0, e1) > GCODE_TOLERANCE)                   // If the Manhattan distance from the ending point of the current block
        break;                                                                  // to the starting point of the next is significant, break out of the loop;

      index_block = index_block->next;                                          // If the endpoints did meet, keep following the sub-chain;
    }

    start_block->ends (start_block, e0, t, GCODE_GET);                          // Obtain the starting point of the saved first block,
    index_block->ends (index_block, t, e1, GCODE_GET);                          // and the ending point of the last contiguous one;

    closed &= (GCODE_MATH_2D_MANHATTAN (e0, e1) < GCODE_TOLERANCE) ? 1 : 0;     // If they are practically identical, the sketch is still considered 'closed';

    index_block = index_block->next;                                            // Move on the next sub-chain by starting again with the 'unconnected' block;
  }

  return (closed);                                                              // If any of the sub-chains failed to 'close', the sketch fails to 'close' too;
}

gcode_block_t *
gcode_sketch_prev_connected (gcode_block_t *block)
{
  gcode_block_t *index_block;
  gcode_vec2d_t t, e0, e1;
  gfloat_t dist;

  /**
   * Find prev connected block.
   */
  if (block->parent)
  {
    index_block = block->parent->listhead;
  }
  else
  {
    index_block = block->gcode->listhead;
  }

  while (index_block)
  {
    if (index_block != block)
    {
      /* Check if ends are the same pt. */
      block->ends (block, e0, t, GCODE_GET);
      index_block->ends (index_block, t, e1, GCODE_GET);

      dist = GCODE_MATH_2D_DISTANCE (e0, e1);

      if (dist <= GCODE_TOLERANCE)
        return (index_block);
    }

    index_block = index_block->next;
  }

  return (NULL);
}

gcode_block_t *
gcode_sketch_next_connected (gcode_block_t *block)
{
  gcode_block_t *index_block;
  gcode_vec2d_t t, e0, e1;
  gfloat_t dist;

  /**
   * Find next connected block.
   */
  if (block->parent)
  {
    index_block = block->parent->listhead;
  }
  else
  {
    index_block = block->gcode->listhead;
  }

  while (index_block)
  {
    if (index_block != block)
    {
      /* Check if ends are the same pt. */
      block->ends (block, t, e0, GCODE_GET);
      index_block->ends (index_block, e1, t, GCODE_GET);

      dist = GCODE_MATH_2D_DISTANCE (e0, e1);

      if (dist <= GCODE_TOLERANCE)
        return (index_block);
    }

    index_block = index_block->next;
  }

  return (NULL);
}
