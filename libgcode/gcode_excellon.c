/**
 *  gcode_excellon.c
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
#include "gcode_drill_holes.h"
#include "gcode_point.h"
#include "gcode_tool.h"
#include "gcode_arc.h"
#include "gcode_line.h"
#include "gcode_util.h"
#include "gcode.h"

/**
 * Open the Excellon drill file 'filename' and import its contents into a series
 * of alternating 'tool' and 'drill holes' blocks (adding the actual drill holes
 * as points under these), then insert these under the supplied template block;
 */

int
gcode_excellon_import (gcode_block_t *template_block, char *filename)
{
  FILE *fh;
  gcode_t *gcode;
  gcode_block_t *tool_block, *drill_block, *point_block;
  gcode_tool_t *tool;
  gcode_point_t *point;
  gcode_excellon_tool_t *tool_set;
  char *buffer = NULL, buf[10];
  long int length, nomore, index;
  int buf_ind, start, tool_num, tool_ind;

  gcode = template_block->gcode;

  start = 0;
  tool_set = NULL;
  tool_num = 0;
  tool_ind = 0;

  fh = fopen (filename, "r");

  if (!fh)
    return (1);

  fseek (fh, 0, SEEK_END);
  length = ftell (fh);
  fseek (fh, 0, SEEK_SET);
  buffer = (char *)malloc (length);
  nomore = fread (buffer, 1, length, fh);

  index = 0;

  while (index < nomore)
  {
    if (buffer[index] == 'T')
    {
      index++;

      buf[0] = buffer[index];
      buf[1] = buffer[index + 1];
      buf[2] = 0;

      if (start)
      {
        for (tool_ind = 0; tool_ind < tool_num; tool_ind++)
          if (atoi (buf) == tool_set[tool_ind].index)
            break;

        /* Create both a tool and drill holes block */
        gcode_tool_init (&tool_block, gcode, NULL);
        gcode_drill_holes_init (&drill_block, gcode, NULL);

        tool = (gcode_tool_t *)tool_block->pdata;

        tool->diameter = tool_set[tool_ind].diameter;
        tool->feed = 1.0;
        tool->prompt = 1;
        sprintf (tool_block->comment, "%.4f\" drill (T%d)", tool->diameter, tool_set[tool_ind].index);
        sprintf (tool->label, "%.4f\" drill (T%d)", tool->diameter, tool_set[tool_ind].index);

        gcode_append_as_listtail (template_block, tool_block);                  // Append 'tool_block' to the end of 'template_block's list (as head if the list is NULL)
        gcode_append_as_listtail (template_block, drill_block);                 // Append 'drill_block' to the end of 'template_block's list (as head if the list is NULL)
      }
      else
      {
        tool_set = (gcode_excellon_tool_t *) realloc (tool_set, (tool_num + 1) * sizeof (gcode_excellon_tool_t));
        tool_set[tool_num].index = atoi (buf);

        index += 2;

        /* Skip over 'C' */
        index++;

        buf_ind = 0;

        while (buffer[index] != '\n')
        {
          buf[buf_ind] = buffer[index];
          buf_ind++;
          index++;
        }

        tool_set[tool_num].diameter = atof (buf);
        tool_num++;
      }
    }
    else if (buffer[index] == '%')
    {
      if (tool_num > 0)
        start = 1;

      index++;
    }
    else if (buffer[index] == 'X')
    {
      gfloat_t x, y;

      index++;

      buf_ind = 0;

      while (buffer[index] != 'Y')
      {
        buf[buf_ind] = buffer[index];
        buf_ind++;
        index++;
      }

      x = 0.0001 * atof (buf);
      index++;

      buf_ind = 0;

      while (buffer[index] != '\n')
      {
        buf[buf_ind] = buffer[index];
        buf_ind++;
        index++;
      }

      y = 0.0001 * atof (buf);
      index++;

      gcode_point_init (&point_block, gcode, drill_block);

      point = (gcode_point_t *)point_block->pdata;

      point->p[0] = x;
      point->p[1] = y;

      gcode_append_as_listtail (drill_block, point_block);                      // Append 'point_block' to the end of 'drill_block's list (as head if the list is NULL)
    }
    else
    {
      while ((buffer[index] != '\n') && (index < nomore))
        index++;

      index++;
    }
  }

  free (tool_set);
  free (buffer);
  fclose (fh);

  return (0);
}
