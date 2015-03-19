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
#include <stdlib.h>
#include <string.h>
#include "gcode_util.h"
#include "gcode_tool.h"

void
gcode_pocket_init (gcode_pocket_t *pocket, gfloat_t resolution)
{
  pocket->row_num = 0;
  pocket->seg_num = 0;                                                          /* this disappears when you make this algorithm suck less */
  pocket->row_array = NULL;
  pocket->resolution = resolution;
}

void
gcode_pocket_free (gcode_pocket_t *pocket)
{
  int i;

  for (i = 0; i < pocket->row_num; i++)
  {
    free (pocket->row_array[i].line_array);
  }

  free (pocket->row_array);
}

void
gcode_pocket_prep (gcode_pocket_t *pocket, gcode_block_t *start_block, gcode_block_t *end_block)
{
  gcode_block_t *index_block;
  gcode_tool_t *tool;
  gcode_pocket_row_t *row;
  gfloat_t x_array[64], y;
  uint32_t x_index, i;
  gfloat_t y_min, y_max;

  tool = gcode_tool_find (start_block);

  /**
   * Call eval on each block and get the x values.
   * Next, sort the x values.
   * Using odd/even fill/nofill gapping, generate lines to fill the gaps.
   */
  pocket->row_num = (int)(1 + start_block->gcode->material_size[1] / pocket->resolution);

  pocket->row_array = malloc (pocket->row_num * sizeof (gcode_pocket_row_t));

  pocket->row_num = 0;

  y_min = 0.0 - start_block->gcode->material_origin[1];
  y_max = y_min + start_block->gcode->material_size[1];

  for (y = y_min; y <= y_max; y += pocket->resolution)
  {
    row = &pocket->row_array[pocket->row_num];

    row->line_array = malloc (64 * sizeof (gcode_vec2d_t));

    x_index = 0;

    index_block = start_block;

    while (index_block != end_block)
    {
      index_block->eval (index_block, y, x_array, &x_index);
      index_block = index_block->next;
    }

    qsort (x_array, x_index, sizeof (gfloat_t), gcode_util_qsort_compare_asc);
    gcode_util_remove_duplicate_scalars (x_array, &x_index);

    row->line_num = 0;

    /* Generate the Lines */

    if (x_index >= 2)
    {
      for (i = 0; i + 1 < x_index; i += 2)
      {
        row->line_array[row->line_num][0] = x_array[i];
        row->line_array[row->line_num][1] = x_array[i + 1];
        row->line_num++;
        pocket->seg_num++;
      }
    }

    row->y = y;

    pocket->row_num++;
  }
}

void
gcode_pocket_make (gcode_pocket_t *pocket, gcode_block_t *block, gfloat_t depth, gfloat_t touch_z, gcode_tool_t *tool)
{
  gcode_pocket_row_t *row;
  gfloat_t padding;
  int i, j;

  padding = tool->diameter * PADDING_FRACTION;

  if (pocket->seg_num == 0)                                                     // Return if no pocketing is to occur
    return;

  GCODE_NEWLINE (block);

  GCODE_COMMENT (block, "Preliminary Pocket Milling Phase");

  GCODE_NEWLINE (block);

  for (i = 0; i < pocket->row_num; i++)
  {
    row = &pocket->row_array[i];

    /* Zig Zag */
    if (i % 2)
    {
      /* Right to Left */
      for (j = row->line_num - 1; j >= 0; j--)
      {
        if (fabs (row->line_array[j][0] - row->line_array[j][1]) < tool->diameter)
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
         */

        GCODE_RETRACT (block, block->gcode->ztraverse);

        GCODE_2D_MOVE (block, row->line_array[j][1] - padding, row->y, "");

        if (touch_z >= depth)
        {
          GCODE_PLUMMET (block, touch_z);
        }

        GCODE_DESCEND (block, depth, tool);

        GCODE_2D_LINE (block, row->line_array[j][0] + padding, row->y, "");
      }
    }
    else
    {
      /* Left to Right */
      for (j = 0; j < row->line_num; j++)
      {
        if (fabs (row->line_array[j][0] - row->line_array[j][1]) < tool->diameter)
          continue;

        /**
         * This retract exists because it is not guaranteed that the next pass
         *  of the zig-zag will not remove material that should remain.
         */

        GCODE_RETRACT (block, block->gcode->ztraverse);

        GCODE_2D_MOVE (block, row->line_array[j][0] + padding, row->y, "");

        if (touch_z >= depth)
        {
          GCODE_PLUMMET (block, touch_z);
        }

        GCODE_DESCEND (block, depth, tool);

        GCODE_2D_LINE (block, row->line_array[j][1] - padding, row->y, "");
      }
    }
  }

  GCODE_RETRACT (block, block->gcode->ztraverse);
}

void
gcode_pocket_subtract (gcode_pocket_t *pocket_a, gcode_pocket_t *pocket_b)
{
  gcode_pocket_row_t *row_a;
  gcode_pocket_row_t *row_b;
  int i, j, k, l;

  for (i = 0; i < pocket_a->row_num; i++)
  {
    row_a = &pocket_a->row_array[i];
    row_b = &pocket_b->row_array[i];

    /**
     * Compare each line in pocket_a with each line in pocket_b.
     * If there is an overlap of a line from pocket_b with a line in pocket_a
     * then subtract it from pocket_a.
     */

    for (j = 0; j < row_a->line_num; j++)
    {
      for (k = 0; k < row_b->line_num; k++)
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
          for (l = row_a->line_num - 1; l > j; l--)
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
          row_a->line_num++;

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
