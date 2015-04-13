/**
 *  gcode_end.c
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

#include "gcode_end.h"

void
gcode_end_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_end_t *end;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_END, GCODE_FLAGS_LOCK);

  (*block)->free = gcode_end_free;
  (*block)->save = gcode_end_save;
  (*block)->load = gcode_end_load;
  (*block)->make = gcode_end_make;
  (*block)->scale = gcode_end_scale;
  (*block)->parse = gcode_end_parse;

  (*block)->pdata = malloc (sizeof (gcode_end_t));;

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Shutdown Mill");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  end = (gcode_end_t *)(*block)->pdata;

  end->retract_position[0] = gcode->material_origin[0];
  end->retract_position[1] = gcode->material_origin[1];
  end->retract_position[2] = gcode->material_origin[2] + GCODE_UNITS ((*block)->gcode, 1.0);

  end->home_all_axes = (gcode->machine_options & GCODE_MACHINE_OPTION_HOME_SWITCHES) == 0 ? 0 : 1;
}

void
gcode_end_free (gcode_block_t **block)
{
  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_end_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_end_t *end;
  uint32_t size;
  uint8_t data;

  end = (gcode_end_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_END);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_3D_FLT (fh, GCODE_XML_ATTR_END_RETRACT_POSITION, end->retract_position);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_END_HOME_ALL_AXES, end->home_all_axes);
    GCODE_WRITE_XML_CL_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_END_RETRACT_POSITION, sizeof (gcode_vec3d_t), end->retract_position);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_END_HOME_ALL_AXES, sizeof (uint8_t), &end->home_all_axes);
  }
}

void
gcode_end_load (gcode_block_t *block, FILE *fh)
{
  gcode_end_t *end;
  uint32_t bsize, dsize, start;
  uint8_t data;

  end = (gcode_end_t *)block->pdata;

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

      case GCODE_BIN_DATA_END_RETRACT_POSITION:
        fread (end->retract_position, sizeof (gcode_vec3d_t), 1, fh);
        break;

      case GCODE_BIN_DATA_END_HOME_ALL_AXES:
        fread (&end->home_all_axes, sizeof (uint8_t), 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_end_make (gcode_block_t *block)
{
  char string[256];
  gcode_end_t *end;

  end = (gcode_end_t *)block->pdata;

  GCODE_CLEAR (block);

  GCODE_NEWLINE (block);

  sprintf (string, "END: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  if (block->gcode->machine_options & GCODE_MACHINE_OPTION_HOME_SWITCHES)
  {
    GCODE_GO_HOME (block, block->gcode->ztraverse);
  }
  else
  {
    GCODE_PULL_UP (block, end->retract_position[2]);

    GCODE_2D_MOVE (block, end->retract_position[0], end->retract_position[1], "move to parking position");
  }

  GCODE_COMMAND (block, "M30", "program end and reset");

  if (block->gcode->driver == GCODE_DRIVER_HAAS)
  {
    GCODE_APPEND (block, "%\n");
  }
}

void
gcode_end_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_end_t *end;

  end = (gcode_end_t *)block->pdata;

  end->retract_position[0] *= scale;
  end->retract_position[1] *= scale;
  end->retract_position[2] *= scale;
}

void
gcode_end_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_end_t *end;

  end = (gcode_end_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_END_RETRACT_POSITION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_3D_FLT (xyz, value))
        for (int j = 0; j < 3; j++)
          end->retract_position[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_END_HOME_ALL_AXES) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        end->home_all_axes = m;
    }
  }
}
