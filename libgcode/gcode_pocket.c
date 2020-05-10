/**
 *  gcode_pocket.c
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

#include "gcode_pocket.h"
#include "gcode_util.h"
#include "gcode_line.h"
#include "gcode_tool.h"
#include "gcode.h"

void
gcode_pocket_init (gcode_pocket_t *pocket, gcode_block_t *target, gcode_tool_t *tool)
{
  pocket->row_count = 0;
  pocket->seg_count = 0;                                                        /* this disappears when you make this algorithm suck less */
  pocket->tool = tool;
  pocket->target = target;
  pocket->first_block = NULL;
  pocket->final_block = NULL;
  pocket->row_array = NULL;
}

void
gcode_pocket_free (gcode_pocket_t *pocket)
{
  int i;

  for (i = 0; i < pocket->row_count; i++)
  {
    free (pocket->row_array[i].line_array);
  }

  free (pocket->row_array);
}

void
gcode_pocket_prep (gcode_pocket_t *pocket, gcode_block_t *first_block, gcode_block_t *final_block)
{
  gcode_t *gcode;
  gcode_block_t *index_block;
  gcode_pocket_row_t *row;
  gfloat_t x_array[1024], y;
  uint32_t x_index, i;
  gfloat_t y_min, y_max;
  gfloat_t y_resolution;

  gcode = pocket->target->gcode;

  pocket->first_block = first_block;
  pocket->final_block = final_block;

  y_resolution = pocket->tool->diameter * 0.5;

  /**
   * Call eval on each block and get the x values. Next, sort the x values.
   * Using odd/even fill/nofill gapping, generate lines to fill the gaps.
   */

  pocket->row_count = (int)(1 + gcode->material_size[1] / y_resolution);

  pocket->row_array = malloc (pocket->row_count * sizeof (gcode_pocket_row_t));

  pocket->row_count = 0;

  y_min = 0.0 - gcode->material_origin[1];
  y_max = y_min + gcode->material_size[1];

  for (y = y_min; y <= y_max; y += y_resolution)
  {
    row = &pocket->row_array[pocket->row_count];

    row->line_array = malloc (64 * sizeof (gcode_vec2d_t));

    x_index = 0;

    index_block = first_block;

    while ((index_block != final_block) && (x_index < MAX_ELEMENTS (x_array) - 2))
    {
      index_block->eval (index_block, y, x_array, &x_index);
      index_block = index_block->next;
    }

    qsort (x_array, x_index, sizeof (gfloat_t), gcode_util_qsort_compare_asc);
    gcode_util_remove_duplicate_scalars (x_array, &x_index);

    row->line_count = 0;

    /* Generate the Lines */

    if (x_index >= 2)
    {
      for (i = 0; i + 1 < x_index; i += 2)
      {
        row->line_array[row->line_count][0] = x_array[i];
        row->line_array[row->line_count][1] = x_array[i + 1];
        row->line_count++;
        pocket->seg_count++;
      }
    }

    row->y = y;

    pocket->row_count++;
  }
}

/**
 * This returns a TRUE/FALSE determination of whether the depth the tool is at
 * now (stored in the gcode referenced by 'pocket') is equal to 'z' or not;
 */

static int
now_at_depth (gcode_pocket_t *pocket, gfloat_t z)
{
  if (GCODE_MATH_IS_EQUAL (pocket->target->gcode->tool_zpos, z))
    return (1);
  else
    return (0);
}

/**
 * This returns a TRUE/FALSE determination of whether it is possible to travel
 * from the current tool position (stored in the gcode of the pocket's target)
 * to (x,y) in a straight line without ever hitting the contour of 'pocket';
 * unfortunately, finding that out that implies intersecting every segment of
 * the contour with the line starting where the tool is and ending in (x,y);
 * NOTE: pockets are created ('prepped') based on a list of blocks that are
 * already offset according to the depth 'z' the pocket is meant to be milled
 * out at AND according to the appropriate tool radius; pockets keep pointers
 * to that block list which is assumed to a) still exist and b) be linked to
 * an offset of zero, exactly in order to be able to perform this check;
 */

static int
path_within_pocket (gcode_pocket_t *pocket, gfloat_t x, gfloat_t y)
{
  gcode_t *gcode;
  gcode_block_t *line_block;
  gcode_block_t *index_block;
  gcode_vec2d_t p0, p1;
  gcode_vec2d_t lmin, lmax;
  gcode_vec2d_t bmin, bmax;
  gcode_vec2d_t ip_array[2];
  int ip_count;
  int result;

  gcode = pocket->target->gcode;

  p0[0] = gcode->tool_xpos;                                                     // The starting point 'p0' is the current position of the tool;
  p0[1] = gcode->tool_ypos;

  p1[0] = x;                                                                    // The target point 'p1' is the (x,y) coordinates supplied;
  p1[1] = y;

  result = TRUE;                                                                // The path is presumed viable unless proven otherwise;

  gcode_line_init (&line_block, gcode, NULL);                                   // We need the create a new line block from p0 to p1 to calculate on;

  line_block->ends (line_block, p0, p1, GCODE_SET);                             // Set the endpoints of the new line to p0 and p1;

  gcode_line_qdbb (line_block, lmin, lmax);                                     // Get a quick-and-dirty bounding box for the line;

  index_block = pocket->first_block;                                            // Traverse the source block list starting from the saved reference;

  while (index_block != pocket->final_block)
  {
    gcode_util_qdbb (index_block, bmin, bmax);                                  // Get a quick-and-dirty bounding box for the current block too;

    if (!GCODE_MATH_IS_APART (lmin, lmax, bmin, bmax))                          // If the bounding boxes clear each other, don't even try to intersect;
    {
      if (gcode_util_intersect (line_block, index_block, ip_array, &ip_count) == 0)
      {
        gfloat_t dist0, dist1;                                                  // So we do have intersection - that in itself does not rule out this path;

        dist0 = GCODE_MATH_2D_DISTANCE (p0, ip_array[0]);                       // The intersection point is compared to the proposed path's endpoints;
        dist1 = GCODE_MATH_2D_DISTANCE (p1, ip_array[0]);

        if ((dist0 > GCODE_PRECISION) && (dist1 > GCODE_PRECISION))             // If it coincides with neither of them though, this path would cross outside
          result = FALSE;                                                       // the pocket's contour removing material that it should not: it's not viable.

        if (ip_count > 1)                                                       // If there's a second intersection point, it's compared to the endpoints too;
        {
          dist0 = GCODE_MATH_2D_DISTANCE (p0, ip_array[1]);
          dist1 = GCODE_MATH_2D_DISTANCE (p1, ip_array[1]);

          if ((dist0 > GCODE_PRECISION) && (dist1 > GCODE_PRECISION))           // The same logic applies: if it's not one of the ends of the proposed path,
            result = FALSE;                                                     // this path is not viable.
        }
      }
    }

    index_block = index_block->next;                                            // Move on to the next block in the pocket contour;
  }

  line_block->free (&line_block);                                               // Dispose of the line block we created;

  return (result);
}

static void
pocket_make_traditional (gcode_pocket_t *pocket, gfloat_t z, gfloat_t touch_z)
{
  gcode_pocket_row_t *row;
  gcode_block_t *block;
  gfloat_t x0, x1, y;
  gfloat_t travel_z;
  gfloat_t padding;
  int row_index;
  int line_index;

  block = pocket->target;

  travel_z = block->gcode->ztraverse;

  padding = pocket->tool->diameter * PADDING_FRACTION;

  if (pocket->seg_count == 0)                                                   // Return if no pocketing is to occur
    return;

  GCODE_NEWLINE (block);

  GCODE_COMMENT (block, "Preliminary Pocket Milling Phase, Strategy: Traditional");

  GCODE_NEWLINE (block);

  for (row_index = 0; row_index < pocket->row_count; row_index++)
  {
    row = &pocket->row_array[row_index];

    y = row->y;

    /* Zig Zag */
    if (row_index % 2)
    {
      /* Right to Left */
      for (line_index = row->line_count - 1; line_index >= 0; line_index--)
      {
        x0 = row->line_array[line_index][1] - padding;                          // We are moving on this row "in reverse" so yeah, that's not a mistake:
        x1 = row->line_array[line_index][0] + padding;                          // we really go from x0 = line[1] - padding to x1 = line[0] + padding;

        if (fabs (x1 - x0) < pocket->tool->diameter)                            // If the tool won't fit between the line endpoints, maybe we shouldn't use it;
          continue;

        /**
         * This retract exists because it is not guaranteed that the next pass
         * of the zig-zag will not remove material that should remain, e.g.
         * +---------------+
         * +---*********---+
         * +------***------+
         * +-*************-+
         * +---------------+
         * where "*" is the path of the end-mill.
         * NOTE: GCODE_MOVE_TO DOES imply a retract unless already at (x0,y,z)
         */

        GCODE_MOVE_TO (block, x0, y, z, travel_z, touch_z, pocket->tool, "next segment");

        GCODE_2D_LINE (block, x1, y, "");
      }
    }
    else
    {
      /* Left to Right */
      for (line_index = 0; line_index < row->line_count; line_index++)
      {
        x0 = row->line_array[line_index][0] + padding;                          // This time we are moving on this row "normally", which means
        x1 = row->line_array[line_index][1] - padding;                          // we go from x0 = line[0] + padding to x1 = line[1] - padding;

        if (fabs (x1 - x0) < pocket->tool->diameter)                            // If the tool won't fit between the line endpoints, maybe we shouldn't use it;
          continue;

        /**
         * This retract exists because it is not guaranteed that the next pass
         * of the zig-zag will not remove material that should remain.
         * NOTE: GCODE_MOVE_TO DOES imply a retract unless already at (x0,y,z)
         */

        GCODE_MOVE_TO (block, x0, y, z, travel_z, touch_z, pocket->tool, "next segment");

        GCODE_2D_LINE (block, x1, y, "");
      }
    }
  }

  GCODE_RETRACT (block, travel_z);
}

static void
pocket_make_alternate_1 (gcode_pocket_t *pocket, gfloat_t z, gfloat_t touch_z)
{
  gcode_pocket_row_t *row;
  gcode_block_t *block;
  gfloat_t x0, x1, y;
  gfloat_t current_z;
  gfloat_t travel_z;
  gfloat_t padding;
  int row_index;
  int line_index;
  int row_start;
  int at_depth;

  block = pocket->target;

  travel_z = block->gcode->ztraverse;

  padding = pocket->tool->diameter * PADDING_FRACTION;

  if (pocket->seg_count == 0)                                                   // Return if no pocketing is to occur
    return;

  GCODE_NEWLINE (block);

  GCODE_COMMENT (block, "Preliminary Pocket Milling Phase, Strategy: Serpentine");

  GCODE_NEWLINE (block);

  GCODE_RETRACT (block, travel_z);                                              // Just to be on the safe side (also useful for "first move" detection)

  for (row_index = 0; row_index < pocket->row_count; row_index++)
  {
    row = &pocket->row_array[row_index];

    y = row->y;

    /* Zig Zag */
    if (row_index % 2)
    {
      /* Right to Left */
      for (line_index = row->line_count - 1; line_index >= 0; line_index--)
      {
        row_start = (line_index == row->line_count - 1) ? TRUE : FALSE;         // TRUE if this is the first segment on this row

        x0 = row->line_array[line_index][1] - padding;                          // We are moving on this row "in reverse" so yeah, that's not a mistake:
        x1 = row->line_array[line_index][0] + padding;                          // we really go from x0 = line[1] - padding to x1 = line[0] + padding;

        if (fabs (x1 - x0) < pocket->tool->diameter)                            // If the tool won't fit between the line endpoints, maybe we shouldn't use it;
          continue;

        /**
         * - if this is not the beginning of a new row (thou shalt not try to
         *   travel in-plane between segments of the same row - multiple segs
         *   by definition mean that THERE'S SOMETHING BETWEEN HERE AND THERE)
         * OR
         * - if the tool is not at the current working depth "z" (thou shalt
         *   not conflate traveling "in-plane" and "in-some-other-plane")
         * OR
         * - if the tool moving to (x0, y) at z crosses the pocket perimeter
         *   (thou shalt not mow down internal corners while relocating)
         * THEN we must properly retract / move / plunge using GCODE_MOVE_TO;
         * Otherwise though, we can just travel in-plane using GCODE_2D_LINE.
         */

        if (row_start && now_at_depth (pocket, z) && path_within_pocket (pocket, x0, y))
        {
          GCODE_2D_LINE (block, x0, y, "");
        }
        else
        {
          GCODE_MOVE_TO (block, x0, y, z, travel_z, touch_z, pocket->tool, "next segment");
        }

        GCODE_2D_LINE (block, x1, y, "");
      }
    }
    else
    {
      /* Left to Right */
      for (line_index = 0; line_index < row->line_count; line_index++)
      {
        row_start = (line_index == 0) ? TRUE : FALSE;                           // TRUE if this is the first segment on this row

        x0 = row->line_array[line_index][0] + padding;                          // This time we are moving on this row "normally", which means
        x1 = row->line_array[line_index][1] - padding;                          // we go from x0 = line[0] + padding to x1 = line[1] - padding;

        if (fabs (x1 - x0) < pocket->tool->diameter)                            // If the tool won't fit between the line endpoints, maybe we shouldn't use it;
          continue;

        /**
         * If there's any obstruction between where we are and (x0, y) and/or
         * we're not at the proper z depth (see details at the branch above)
         * then we must properly retract / move / plunge using GCODE_MOVE_TO;
         * Otherwise though, we can just travel in-plane using GCODE_2D_LINE.
         */

        if (row_start && now_at_depth (pocket, z) && path_within_pocket (pocket, x0, y))
        {
          GCODE_2D_LINE (block, x0, y, "");
        }
        else
        {
          GCODE_MOVE_TO (block, x0, y, z, travel_z, touch_z, pocket->tool, "next segment");
        }

        GCODE_2D_LINE (block, x1, y, "");
      }
    }
  }

  GCODE_RETRACT (block, travel_z);
}

void
gcode_pocket_make (gcode_pocket_t *pocket, gfloat_t z, gfloat_t touch_z)
{
  gcode_t *gcode;

  gcode = pocket->target->gcode;

  switch (gcode->pocketing_style)
  {
    case GCODE_POCKETING_TRADITIONAL:

      pocket_make_traditional (pocket, z, touch_z);

      break;

    case GCODE_POCKETING_ALTERNATE_1:

      pocket_make_alternate_1 (pocket, z, touch_z);

      break;
  }
}

void
gcode_pocket_subtract (gcode_pocket_t *pocket_a, gcode_pocket_t *pocket_b)
{
  gcode_pocket_row_t *row_a;
  gcode_pocket_row_t *row_b;
  int i, j, k, l;

  for (i = 0; i < pocket_a->row_count; i++)
  {
    row_a = &pocket_a->row_array[i];
    row_b = &pocket_b->row_array[i];

    /**
     * Compare each line in pocket_a with each line in pocket_b.
     * If there is an overlap of a line from pocket_b with a line in pocket_a
     * then subtract it from pocket_a.
     */

    for (j = 0; j < row_a->line_count; j++)
    {
      for (k = 0; k < row_b->line_count; k++)
      {
        /**
         * CASE 1: *---+---+---*   contained within (or complete overlap)
         * CASE 2: *---+---*---+   overlap right
         * CASE 3: +---*---+---*   overlap left
         * Where '*' is pocket_a and '+' is pocket_b.
         */

        if (row_b->line_array[k][0] + GCODE_PRECISION >= row_a->line_array[j][0] &&
            row_b->line_array[k][0] - GCODE_PRECISION <= row_a->line_array[j][1] &&
            row_b->line_array[k][1] + GCODE_PRECISION >= row_a->line_array[j][0] &&
            row_b->line_array[k][1] - GCODE_PRECISION <= row_a->line_array[j][1])
        {
          /* CASE 1: split into 2 lines, shift all lines up one, insert new line into free slot */
          for (l = row_a->line_count - 1; l > j; l--)
          {
            row_a->line_array[l + 1][0] = row_a->line_array[l][0];
            row_a->line_array[l + 1][1] = row_a->line_array[l][1];
          }

          /* Line j is now a free slot, everything has been shifted up 1 slot */
          row_a->line_array[j + 1][0] = row_b->line_array[k][1];
          row_a->line_array[j + 1][1] = row_a->line_array[j][1];

          /* trim existing line to beginning of overlapping line */
          row_a->line_array[j][1] = row_b->line_array[k][0];

          /* Increment the number of lines */
          row_a->line_count++;

          /* Increment to newly created line */
          j++;
        }
        else if (row_b->line_array[k][0] > row_a->line_array[j][0] &&
                 row_b->line_array[k][0] < row_a->line_array[j][1])
        {
          /* CASE 2 */
          row_a->line_array[j][1] = row_b->line_array[k][0];
        }
        else if (row_b->line_array[k][1] > row_a->line_array[j][0] &&
                 row_b->line_array[k][1] < row_a->line_array[j][1])
        {
          /* CASE 3 */
          row_a->line_array[j][0] = row_b->line_array[k][1];
        }
      }
    }
  }
}
