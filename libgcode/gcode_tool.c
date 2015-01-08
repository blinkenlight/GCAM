/**
 *  gcode_tool.c
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

#include "gcode_tool.h"

void
gcode_tool_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_tool_t *tool;

  *block = (gcode_block_t *)malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_TOOL, 0);

  (*block)->free = gcode_tool_free;
  (*block)->make = gcode_tool_make;
  (*block)->save = gcode_tool_save;
  (*block)->load = gcode_tool_load;
  (*block)->clone = gcode_tool_clone;
  (*block)->scale = gcode_tool_scale;
  (*block)->parse = gcode_tool_parse;

  (*block)->pdata = malloc (sizeof (gcode_tool_t));

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Tool Change");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  tool = (gcode_tool_t *)(*block)->pdata;

  strcpy (tool->label, "");

  tool->diameter = 0;
  tool->length = 0;
  tool->prompt = 0;
  tool->feed = 0;
  tool->change_position[0] = 0.0;
  tool->change_position[1] = 0.0;
  tool->change_position[2] = GCODE_UNITS ((*block)->gcode, 1.0);
  tool->number = 1;
  tool->plunge_ratio = 0.2;                                                     /* 20% */
  tool->spindle_rpm = 2000;                                                     /* 2,000 RPM */
  tool->coolant = ((*block)->gcode->machine_options & GCODE_MACHINE_OPTION_COOLANT) == 0 ? 0 : 1;

  gcode_tool_calc (*block);
}

void
gcode_tool_free (gcode_block_t **block)
{
  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_tool_calc (gcode_block_t *block)
{
  gcode_tool_t *tool;

  tool = (gcode_tool_t *)block->pdata;

  switch (block->gcode->material_type)
  {
    case GCODE_MATERIAL_ALUMINUM:
      tool->feed = 3.0;
      tool->plunge_ratio = 0.2;
      break;

    case GCODE_MATERIAL_FOAM:
      tool->feed = 15.0;
      tool->plunge_ratio = 1.0;
      break;

    case GCODE_MATERIAL_PLASTIC:
      tool->feed = 7.0;
      tool->plunge_ratio = 1.0;
      break;

    case GCODE_MATERIAL_STEEL:
      tool->feed = 0.1;
      tool->plunge_ratio = 0.1;
      break;

    case GCODE_MATERIAL_WOOD:
      tool->feed = 8.0;
      tool->plunge_ratio = 0.5;
      break;
  }

  if (block->gcode->units == GCODE_UNITS_MILLIMETER)
    tool->feed *= GCODE_INCH2MM;
}

void
gcode_tool_make (gcode_block_t *block)
{
  gcode_tool_t *tool;
  char string[256];

  tool = (gcode_tool_t *)block->pdata;

  GCODE_CLEAR (block);

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  GCODE_APPEND (block, "\n");

  sprintf (string, "TOOL CHANGE: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_APPEND (block, "\n");

  sprintf (string, "Selected Tool: %s", tool->label);
  GCODE_COMMENT (block, string);

  sprintf (string, "Tool Diameter: %f", tool->diameter);
  GCODE_COMMENT (block, string);

  if (tool->prompt)
  {
    GCODE_PULL_UP (block, tool->change_position[2]);

    GCODE_2D_MOVE (block, tool->change_position[0], tool->change_position[1], "move to tool change position");
  }

  if (block->gcode->machine_options & GCODE_MACHINE_OPTION_SPINDLE_CONTROL)
  {
    GCODE_COMMAND (block, "M05", "spindle off");
  }

  if (tool->prompt || (block->gcode->machine_options & GCODE_MACHINE_OPTION_AUTOMATIC_TOOL_CHANGE))
  {
    sprintf (string, "M06 T%.2d", tool->number);
    GCODE_COMMAND (block, string, tool->label);
  }

  if (block->gcode->machine_options & GCODE_MACHINE_OPTION_SPINDLE_CONTROL)
  {
    GCODE_S_VALUE (block, tool->spindle_rpm, "set spindle speed");

    GCODE_COMMAND (block, "M03", "spindle on");
  }

  if (block->gcode->machine_options & GCODE_MACHINE_OPTION_COOLANT)
  {
    if (tool->coolant)
    {
      GCODE_COMMAND (block, "M08", "coolant on");
    }
    else
    {
      GCODE_COMMAND (block, "M09", "coolant off");
    }
  }

  GCODE_F_VALUE (block, tool->feed, "set feed rate");
}

void
gcode_tool_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_tool_t *tool;

  tool = (gcode_tool_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_TOOL);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_TOOL_DIAMETER, tool->diameter);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_TOOL_LENGTH, tool->length);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_TOOL_PROMPT, tool->prompt);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_TOOL_LABEL, tool->label);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_TOOL_FEED, tool->feed);
    GCODE_WRITE_XML_ATTR_3D_FLT (fh, GCODE_XML_ATTR_TOOL_CHANGE_POSITION, tool->change_position);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_TOOL_NUMBER, tool->number);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_TOOL_PLUNGE_RATIO, tool->plunge_ratio);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_TOOL_SPINDLE_RPM, tool->spindle_rpm);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_TOOL_COOLANT, tool->coolant);
    GCODE_WRITE_XML_CL_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_DIAMETER, sizeof (gfloat_t), &tool->diameter);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_LENGTH, sizeof (gfloat_t), &tool->length);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_PROMPT, sizeof (uint8_t), &tool->prompt);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_LABEL, 32, tool->label);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_FEED, sizeof (gfloat_t), &tool->feed);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_CHANGE_POSITION, 3 * sizeof (gfloat_t), &tool->change_position);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_NUMBER, sizeof (uint8_t), &tool->number);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_PLUNGE_RATIO, sizeof (gfloat_t), &tool->plunge_ratio);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_SPINDLE_RPM, sizeof (uint32_t), &tool->spindle_rpm);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TOOL_COOLANT, sizeof (uint8_t), &tool->coolant);
  }
}

void
gcode_tool_load (gcode_block_t *block, FILE *fh)
{
  gcode_tool_t *tool;
  uint32_t bsize, dsize, start;
  uint8_t data;

  tool = (gcode_tool_t *)block->pdata;

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

      case GCODE_BIN_DATA_TOOL_DIAMETER:
        fread (&tool->diameter, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_LENGTH:
        fread (&tool->length, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_PROMPT:
        fread (&tool->prompt, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_LABEL:
        fread (tool->label, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_FEED:
        fread (&tool->feed, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_CHANGE_POSITION:
        fread (tool->change_position, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_NUMBER:
        fread (&tool->number, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_PLUNGE_RATIO:
        fread (&tool->plunge_ratio, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_SPINDLE_RPM:
        fread (&tool->spindle_rpm, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TOOL_COOLANT:
        fread (&tool->coolant, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_tool_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_tool_t *tool;

  tool = (gcode_tool_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_DIAMETER) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        tool->diameter = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_LENGTH) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        tool->length = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_PROMPT) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        tool->prompt = m;
    }
    if (strcmp (name, GCODE_XML_ATTR_TOOL_LABEL) == 0)
    {
      GCODE_PARSE_XML_ATTR_STRING (tool->label, value);
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_FEED) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        tool->feed = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_CHANGE_POSITION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_3D_FLT (xyz, value))
        for (int j = 0; j < 3; j++)
          tool->change_position[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_NUMBER) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        tool->number = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_PLUNGE_RATIO) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        tool->plunge_ratio = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_SPINDLE_RPM) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        tool->spindle_rpm = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_TOOL_COOLANT) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        tool->coolant = m;
    }
  }
}

void
gcode_tool_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_tool_t *tool, *model_tool;

  model_tool = (gcode_tool_t *)model->pdata;

  gcode_tool_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  tool = (gcode_tool_t *)(*block)->pdata;

  strcpy (tool->label, model_tool->label);

  tool->diameter = model_tool->diameter;
  tool->length = model_tool->length;
  tool->hinc = model_tool->hinc;
  tool->vinc = model_tool->vinc;
  tool->feed = model_tool->feed;
  tool->prompt = model_tool->prompt;
  tool->change_position[0] = model_tool->change_position[0];
  tool->change_position[1] = model_tool->change_position[1];
  tool->change_position[2] = model_tool->change_position[2];
  tool->number = model_tool->number;
  tool->plunge_ratio = model_tool->plunge_ratio;
  tool->spindle_rpm = model_tool->spindle_rpm;
  tool->coolant = model_tool->coolant;
}

void
gcode_tool_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_tool_t *tool;

  tool = (gcode_tool_t *)block->pdata;

  tool->diameter *= scale;
  tool->length *= scale;
  tool->feed *= scale;
  tool->change_position[0] *= scale;
  tool->change_position[1] *= scale;
  tool->change_position[2] *= scale;
}

/**
 * Locate the nearest most previous tool by walking the list backwards and using recursion.
 */
gcode_tool_t *
gcode_tool_find (gcode_block_t *block)
{
  gcode_block_t *index_block;

  index_block = block;

  while (index_block)
  {
    if (index_block->type == GCODE_TYPE_TOOL)
      return ((gcode_tool_t *)index_block->pdata);
    index_block = index_block->prev;
  }

  /* Next, try searching the parent list */
  if (block->parent)
    return (gcode_tool_find (block->parent));

  return (NULL);
}
