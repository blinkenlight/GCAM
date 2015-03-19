/**
 *  gcode_begin.c
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

#include "gcode_begin.h"
#include <time.h>

void
gcode_begin_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_begin_t *begin;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_BEGIN, GCODE_FLAGS_LOCK);

  (*block)->free = gcode_begin_free;
  (*block)->save = gcode_begin_save;
  (*block)->load = gcode_begin_load;
  (*block)->make = gcode_begin_make;
  (*block)->parse = gcode_begin_parse;

  (*block)->pdata = malloc (sizeof (gcode_begin_t));

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Initialize Mill");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  begin = (gcode_begin_t *)(*block)->pdata;

  begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_NONE;
}

void
gcode_begin_free (gcode_block_t **block)
{
  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_begin_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_begin_t *begin;
  uint32_t size;
  uint8_t data;

  begin = (gcode_begin_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_BEGIN);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_BEGIN_COORDINATE_SYSTEM, begin->coordinate_system);
    GCODE_WRITE_XML_CL_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BEGIN_COORDINATE_SYSTEM, sizeof (uint8_t), &begin->coordinate_system);
  }
}

void
gcode_begin_load (gcode_block_t *block, FILE *fh)
{
  gcode_begin_t *begin;
  uint32_t bsize, dsize, start;
  uint8_t data;

  begin = (gcode_begin_t *)block->pdata;

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

      case GCODE_BIN_DATA_BEGIN_COORDINATE_SYSTEM:
        fread (&begin->coordinate_system, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_begin_make (gcode_block_t *block)
{
  gcode_begin_t *begin;
  char string[256], nrcode[32], date_string[32];
  time_t timer;

  begin = (gcode_begin_t *)block->pdata;

  GCODE_CLEAR (block);

  if (block->gcode->driver == GCODE_DRIVER_HAAS)
  {
    GCODE_APPEND (block, "%\n");

    sprintf (string, "O%.5d\n", block->gcode->project_number);
    GCODE_APPEND (block, string);
  }

  sprintf (string, "Project: %s", block->gcode->name);
  GCODE_COMMENT (block, string);

  timer = time (NULL);
  strcpy (date_string, asctime (localtime (&timer)));
  date_string[strlen (date_string) - 1] = 0;
  sprintf (string, "Created: %s with GCAM SE v%s", date_string, VERSION);
  GCODE_COMMENT (block, string);

  gsprintf (string, block->gcode->decimals, "Material Size: X=%z Y=%z Z=%z", block->gcode->material_size[0], block->gcode->material_size[1], block->gcode->material_size[2]);
  GCODE_COMMENT (block, string);

  gsprintf (string, block->gcode->decimals, "Origin Offset: X=%z Y=%z Z=%z", block->gcode->material_origin[0], block->gcode->material_origin[1], block->gcode->material_origin[2]);
  GCODE_COMMENT (block, string);

  sprintf (string, "Notes: %s", block->gcode->notes);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  sprintf (string, "BEGIN: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  if (begin->coordinate_system == GCODE_BEGIN_COORDINATE_SYSTEM_NONE)
  {
    GCODE_COMMENT (block, "Machine coordinates");
  }
  else
  {
    sprintf (nrcode, "G%d", 53 + begin->coordinate_system);
    sprintf (string, "workspace %d", begin->coordinate_system);
    GCODE_COMMAND (block, nrcode, string);
  }

  if (block->gcode->units == GCODE_UNITS_INCH)
  {
    GCODE_COMMAND (block, "G20", "units are inches");
  }
  else
  {
    GCODE_COMMAND (block, "G21", "units are millimeters");
  }

  GCODE_COMMAND (block, "G90", "absolute positioning");
}

void
gcode_begin_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_begin_t *begin;

  begin = (gcode_begin_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_BEGIN_COORDINATE_SYSTEM) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        begin->coordinate_system = m;
    }
  }
}
