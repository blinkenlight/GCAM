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
#include <stdio.h>
#include <string.h>
#include <libgen.h>

/**
 * Open the Excellon drill file 'filename' and import its contents into a series
 * of alternating 'tool' and 'drill holes' blocks (adding the actual drill holes
 * as points under these), then insert these under the supplied template block;
 * NOTE: to make this exceedingly clear - this is NOT a full-featured Excellon
 * parser, and most special features are either not supported or even full-on
 * break the parser (like tool settings other than diameter, operations other
 * than drilling, incomplete XY pairs, canned cycles etc.); the idea was to get
 * it to work with usual machine-generated drill files, not to make it perfect.
 */

int
gcode_excellon_import (gcode_block_t *template_block, char *filename)
{
  FILE *fh;
  size_t size;
  char *buffer;
  gcode_t *gcode;
  gcode_tool_t *tool;
  gcode_point_t *point;
  gcode_block_t *tool_block;
  gcode_block_t *drill_block;
  gcode_block_t *point_block;
  int tool_count, tool_index;
  gcode_excellon_tool_t *tool_set;
  gfloat_t digit_guess, digit_scale, unit_scale;
  long int length, nomore, index;
  int body, line, ignore, result;
  float value1, value2, value;
  long int number1, number2;
  int number;
  char letter;

  body = 0;
  line = 1;
  ignore = 1;
  result = 0;

  digit_guess = 0.0;
  digit_scale = 0.0;
  unit_scale = 0.0;

  tool_set = NULL;
  tool_count = 0;

  gcode = template_block->gcode;

  fh = fopen (filename, "r");

  if (!fh)
  {
    REMARK ("Failed to open file '%s'\n", basename (filename));
    return (1);
  }

  fseek (fh, 0, SEEK_END);                                                      // Read the entire file into the newly allocated 'buffer';
  length = ftell (fh);

  if (!length)                                                                  // A zero length file is not an error, strictly speaking;
  {
    fclose (fh);
    return (0);
  }

  buffer = malloc (length + 1);

  if (!buffer)
  {
    REMARK ("Failed to allocate memory for Excellon import buffer\n");
    fclose (fh);
    return (1);
  }

  fseek (fh, 0, SEEK_SET);
  nomore = fread (buffer, 1, length, fh);
  fclose (fh);

  buffer[nomore] = '\0';                                                        // This lets us use string functions over the buffer;

  index = strspn (buffer, " \t");                                               // Find the first neither-space-nor-tab character;

  while (index < nomore)                                                        // Loop until the end of the buffer is reached;
  {
    if (body == 0)                                                              // There should only be 'header' stuff here;
    {
      switch (buffer[index])
      {
        case ';':                                                               // *** Looking for format clues in the comments;

          if (sscanf (&buffer[index], "; FORMAT = { %*1c:%1c /", &letter) == 1)
          {
            if (letter == '-')                                                  // This is the KiCAD "decimal" mode ("-:-"), no scaling;
              digit_guess = 1.0;
            else if (letter == '2')
              digit_guess = 0.01;
            else if (letter == '3')
              digit_guess = 0.001;
            else if (letter == '4')
              digit_guess = 0.0001;
          }

          break;

        case 'I':                                                               // *** Looking for an "INCH,TZ" statement;

          if (strncmp (&buffer[index], "INCH", 4) == 0)                         // If "INCH" is found, set the unit scale;
          {
            if (gcode->units == GCODE_UNITS_MILLIMETER)
              unit_scale = GCODE_INCH2MM;
            else
              unit_scale = 1.0;
          }

          if (sscanf (&buffer[index], "INCH , %cZ", &letter) == 1)
          {
            if (letter == 'T')                                                  // If "TZ" is found, set the digit scale;
            {
              digit_scale = 0.0001;                                             // The only inch format is supposed to be "00.0000";
            }
            else                                                                // If it's NOT "TZ", we're NOT happy;
            {
              REMARK ("Unsupported Excellon coordinate format (other than 'omit leading zeros')\n");
              result = 1;
            }
          }

          break;

        case 'M':                                                               // *** Looking for a "METRIC,TZ" statement;

          if (strncmp (&buffer[index], "METRIC", 6) == 0)                       // If "METRIC" is found, set the unit scale;
          {
            if (gcode->units == GCODE_UNITS_INCH)
              unit_scale = GCODE_MM2INCH;
            else
              unit_scale = 1.0;
          }

          if (sscanf (&buffer[index], "METRIC , %cZ", &letter) == 1)
          {
            if (letter == 'T')                                                  // If "TZ" is found, set the digit scale;
            {
              digit_scale = 0.001;                                              // The default metric format is allegedly "000.000";
            }
            else                                                                // If it's NOT "TZ", we're NOT happy;
            {
              REMARK ("Unsupported Excellon coordinate format (other than 'omit leading zeros')\n");
              result = 1;
            }
          }

          break;

        case 'T':                                                               // *** Looking for a tool number and diameter definition;

          if (sscanf (&buffer[index], "T %d C %f", &number, &value) == 2)
          {
            if (number >= 0)
            {
              if (unit_scale > 0.0)
              {
                tool_set = realloc (tool_set, (tool_count + 1) * sizeof (gcode_excellon_tool_t));

                if (tool_set)
                {
                  tool_set[tool_count].number = (uint8_t)number;
                  tool_set[tool_count].diameter = (gfloat_t)value * unit_scale;

                  tool_count++;
                }
                else
                {
                  REMARK ("Failed to allocate memory for Excellon tool set\n");
                  result = 1;
                }
              }
              else
              {
                REMARK ("Excellon coordinate unit definition is missing\n");
                result = 1;
              }
            }
            else
            {
              REMARK ("Invalid (negative) Excellon tool definition at line %i\n", line);
              result = 1;
            }
          }

          break;

        case '%':                                                               // *** Found the end of the header section;

          body = 1;

          if (tool_count > 0)
          {
            if (digit_guess > 0.0)
              digit_scale = digit_guess;

            if (digit_scale == 0.0)
            {
              REMARK ("Excellon coordinate format definition is missing\n");
              result = 1;
            }
          }
          else
          {
            REMARK ("No tool definitions found during Excellon import\n");
            result = 1;
          }

          break;
      }
    }
    else                                                                        // There should only be 'body' stuff here;
    {
      switch (buffer[index])
      {
        case 'G':                                                               // *** Looking for Gxx commands (there better not be any...);

          if (sscanf (&buffer[index], "G %2d", &number) == 1)
          {
            if ((number != 5) && (number != 90))                                // "G05" (drill) or "G90" (absolute mode) are ok - the rest, not so much...
            {
              REMARK ("Unsupported Gxx Excellon command at line %i\n", line);
              result = 1;
            }
          }

          break;

        case 'M':                                                               // *** Looking for Mxx commands (there better not be any...);

          if (sscanf (&buffer[index], "M %2d", &number) == 1)
          {
            if (number == 30)                                                   // "M30" : end of program;
            {
              index = nomore;                                                   // Just skip the rest of the file;
            }
            else if (number == 71)                                              // "M71" : mid-body unit switch to metric;
            {
              if (gcode->units == GCODE_UNITS_INCH)
                unit_scale = GCODE_MM2INCH;
              else
                unit_scale = 1.0;
            }
            else if (number == 72)                                              // "M72" : mid-body unit switch to inches;
            {
              if (gcode->units == GCODE_UNITS_MILLIMETER)
                unit_scale = GCODE_INCH2MM;
              else
                unit_scale = 1.0;
            }
            else                                                                // If it's neither "M71" nor "M72" then it can only be one thing - trouble;
            {
              REMARK ("Unsupported Mxx Excellon command at line %i\n", line);
              result = 1;
            }
          }

          break;

        case 'T':                                                               // *** Looking for a tool number selection;

          if (sscanf (&buffer[index], "T %d", &number) == 1)
          {
            if (number >= 0)                                                    // Negative tool numbers are somewhat frowned upon around here, young man...
            {
              for (tool_index = 0; tool_index < tool_count; tool_index++)       // Look for the specified tool: if 'index' reaches 'count', there was no match;
                if (tool_set[tool_index].number == number)
                  break;

              if (tool_index == tool_count)                                     // This absolutely SHOULD be an error but then KiCAD drill files wouldn't work;
              {
                ignore = 1;                                                     // Further points are / keep being ignored until the next valid tool selection;
              }
              else                                                              // Ah, how nice - selection of a tool that actually has been defined first...!
              {
                ignore = 0;                                                     // Further points should be added to a drill holes block preceded by this tool;

                gcode_tool_init (&tool_block, gcode, NULL);
                gcode_drill_holes_init (&drill_block, gcode, NULL);

                tool = (gcode_tool_t *)tool_block->pdata;

                tool->diameter = tool_set[tool_index].diameter;                 // This value is already in the native unit of the gcode / project;
                tool->number = tool_set[tool_index].number;                     // Technically, the tool HAS a number but it doesn't match the project toolset;
                tool->prompt = 1;

                snprintf (tool_block->comment, sizeof (tool_block->comment), "%.4f drill (imported T%d)", tool->diameter, tool->number);
                snprintf (tool->label, sizeof (tool->label), "%.4f drill (imported T%d)", tool->diameter, tool->number);
                tool_block->comment[sizeof (tool_block->comment) - 1] = '\0';   // About that 'snprintf' - what do you think happens when 'diameter' ends up having
                tool->label[sizeof (tool->label) - 1] = '\0';                   // 300 digits because, say, you tried to select a tool that was never defined...?!?

                gcode_append_as_listtail (template_block, tool_block);          // Append 'tool_block' to the end of 'template_block's list (as head if the list is NULL)
                gcode_append_as_listtail (template_block, drill_block);         // Append 'drill_block' to the end of 'template_block's list (as head if the list is NULL)
              }
            }
            else
            {
              REMARK ("Invalid (negative) Excellon tool selection at line %i\n", line);
              result = 1;
            }
          }

          break;

        case 'X':                                                               // *** Looking for XY coordinate pairs (for now complete pairs ONLY);

          if (sscanf (&buffer[index], "X %ld Y %ld", &number1, &number2) == 2)  // Integer scan fails if explicit decimals are present so we test for it first;
          {                                                                     // Testing for float first would hide presence or absence of explicit decimals.
            if (unit_scale > 0.0)                                               // If we got this far digit_scale is set, but we still need to test unit_scale;
            {
              gcode_point_init (&point_block, gcode, drill_block);

              point = (gcode_point_t *)point_block->pdata;

              point->p[0] = (gfloat_t)number1 * digit_scale * unit_scale;       // Integer values mean digit scaling must be done;
              point->p[1] = (gfloat_t)number2 * digit_scale * unit_scale;       // Plus we also have to apply any unit conversion.

              gcode_append_as_listtail (drill_block, point_block);              // Append 'point_block' to the end of 'drill_block's list (as head if the list is NULL)
            }
            else
            {
              REMARK ("Excellon coordinate unit definition is missing\n");
              result = 1;
            }
          }
          else if (sscanf (&buffer[index], "X %f Y %f", &value1, &value2) == 2) // Float scan succeeds even without explicit decimals so we test for it second;
          {                                                                     // Testing for float first would hide presence or absence of explicit decimals.
            if (unit_scale > 0.0)                                               // If we got this far digit_scale is set, but we still need to test unit_scale;
            {
              gcode_point_init (&point_block, gcode, drill_block);

              point = (gcode_point_t *)point_block->pdata;

              point->p[0] = (gfloat_t)value1 * unit_scale;                      // Float values means digit scaling is not necessary; 
              point->p[1] = (gfloat_t)value2 * unit_scale;                      // We still have to apply any unit conversion though.

              gcode_append_as_listtail (drill_block, point_block);              // Append 'point_block' to the end of 'drill_block's list (as head if the list is NULL)
            }
            else
            {
              REMARK ("Excellon coordinate unit definition is missing\n");
              result = 1;
            }
          }

          break;
      }
    }

    if (result)                                                                 // If there was an error, stop parsing and bail out;
      break;

    if (index < nomore)                                                         // Advance the index to the next newline character;
      index += strcspn (&buffer[index], "\n");

    if (index < nomore)                                                         // Skip over it and any other space-like characters;
      index += strspn (&buffer[index], " \t\n");

    line++;
  }

  free (buffer);

  return (result);
}
