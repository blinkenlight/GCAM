/**
 *  gcode_extrusion.c
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

#include "gcode_extrusion.h"
#include "gcode_line.h"
#include "gcode.h"

void
gcode_extrusion_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_extrusion_t *extrusion;
  gcode_block_t *line_block;
  gcode_line_t *line;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_EXTRUSION, GCODE_FLAGS_LOCK);

  (*block)->free = gcode_extrusion_free;
  (*block)->save = gcode_extrusion_save;
  (*block)->load = gcode_extrusion_load;
  (*block)->make = gcode_extrusion_make;
  (*block)->draw = gcode_extrusion_draw;
  (*block)->ends = gcode_extrusion_ends;
  (*block)->scale = gcode_extrusion_scale;
  (*block)->parse = gcode_extrusion_parse;
  (*block)->clone = gcode_extrusion_clone;

  (*block)->pdata = malloc (sizeof (gcode_extrusion_t));

  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Extrusion");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  extrusion = (gcode_extrusion_t *)(*block)->pdata;

  extrusion->resolution = fmaxf (0.001, floor (100.0 * gcode->material_size[2]) * 0.001);
  extrusion->cut_side = GCODE_EXTRUSION_INSIDE;

  extrusion->offset.side = 1.0;
  extrusion->offset.tool = 0.0;
  extrusion->offset.eval = 0.0;
  extrusion->offset.rotation = 0.0;
  extrusion->offset.origin[0] = 0.0;
  extrusion->offset.origin[1] = 0.0;
  extrusion->offset.z[0] = 0.0;
  extrusion->offset.z[1] = 0.0;

  (*block)->offref = &extrusion->offset;

  /* Create default line from Z0 to Z-Depth */

  gcode_line_init (&line_block, gcode, *block);

  gcode_append_as_listtail (*block, line_block);                                // At this point, the list of 'block' is NULL; 'line_block' becomes listhead.

  line = (gcode_line_t *)line_block->pdata;

  line->p0[0] = 0.0;
  line->p0[1] = 0.0;
  line->p1[0] = 0.0;
  line->p1[1] = -gcode->material_size[2];
}

void
gcode_extrusion_free (gcode_block_t **block)
{
  gcode_block_t *index_block, *tmp;

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
gcode_extrusion_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_extrusion_t *extrusion;
  uint32_t size, num, marker;
  uint8_t data;

  extrusion = (gcode_extrusion_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_EXTRUSION);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_EXTRUSION_RESOLUTION, extrusion->resolution);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_EXTRUSION_CUT_SIDE, extrusion->cut_side);
    GCODE_WRITE_XML_OP_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    /**
     * In the XML branch the parent does NOT save even the common attributes
     * of its child blocks (as the binary branch does): saving some attributes
     * here and some (custom) attributes in the child would get rather messy
     */

    index_block = block->listhead;

    while (index_block)
    {
      index_block->save (index_block, fh);

      index_block = index_block->next;
    }

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_EXTRUSION);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    num = 0;
    index_block = block->listhead;

    while (index_block)
    {
      num++;

      index_block = index_block->next;
    }

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_EXTRUSION_NUMBER, sizeof (uint32_t), &num);

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

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_EXTRUSION_RESOLUTION, sizeof (gfloat_t), &extrusion->resolution);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_EXTRUSION_CUT_SIDE, sizeof (uint8_t), &extrusion->cut_side);
  }
}

void
gcode_extrusion_load (gcode_block_t *block, FILE *fh)
{
  gcode_extrusion_t *extrusion;
  gcode_block_t *new_block;
  uint32_t bsize, dsize, start, num, i;
  uint8_t data, type;

  extrusion = (gcode_extrusion_t *)block->pdata;

  gcode_list_free (&block->listhead);

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

      case GCODE_BIN_DATA_EXTRUSION_NUMBER:
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

      case GCODE_BIN_DATA_EXTRUSION_RESOLUTION:
        fread (&extrusion->resolution, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_EXTRUSION_CUT_SIDE:
        fread (&extrusion->cut_side, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_extrusion_make (gcode_block_t *block)
{
  GCODE_CLEAR (block);
}

void
gcode_extrusion_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_block_t *index_block;

  glDisable (GL_DEPTH_TEST);

  index_block = block->listhead;

  while (index_block)
  {
    index_block->draw (index_block, selected);
    index_block = index_block->next;
  }
#endif
}

int
gcode_extrusion_ends (gcode_block_t *block, gcode_vec2d_t p0, gcode_vec2d_t p1, uint8_t mode)
{
  gcode_block_t *index_block;
  gfloat_t t[2];

  p0[0] = 0.0;
  p0[1] = 0.0;
  p1[0] = 0.0;
  p1[1] = 0.0;

  if (!block->listhead)
    return (1);

  index_block = block->listhead;

  /* Get Beginning of first block */
  index_block->ends (index_block, p0, t, GCODE_GET);

  while (index_block)
  {
    /* Get End of last block */
    if (!index_block->next)
      index_block->ends (index_block, t, p1, GCODE_GET);

    index_block = index_block->next;
  }

  return (0);
}

void
gcode_extrusion_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_extrusion_t *extrusion;
  gcode_block_t *index_block;

  extrusion = (gcode_extrusion_t *)block->pdata;

  extrusion->resolution *= scale;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->scale)
      index_block->scale (index_block, scale);

    index_block = index_block->next;
  }
}

void
gcode_extrusion_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_extrusion_t *extrusion;

  extrusion = (gcode_extrusion_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_EXTRUSION_RESOLUTION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        extrusion->resolution = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_EXTRUSION_CUT_SIDE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        extrusion->cut_side = m;
    }
  }
}

void
gcode_extrusion_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_extrusion_t *extrusion, *model_extrusion;
  gcode_block_t *index_block, *new_block;

  model_extrusion = (gcode_extrusion_t *)model->pdata;

  gcode_extrusion_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  gcode_list_free (&(*block)->listhead);                                        // This does actually make sense - extrusions come pre-initialized with a default 'line'

  extrusion = (gcode_extrusion_t *)(*block)->pdata;

  extrusion->resolution = model_extrusion->resolution;
  extrusion->offset = model_extrusion->offset;
  extrusion->cut_side = model_extrusion->cut_side;

  index_block = model->listhead;

  while (index_block)
  {
    index_block->clone (&new_block, gcode, index_block);

    gcode_append_as_listtail (*block, new_block);                               // Append 'new_block' to the end of 'block's list (as head if the list is NULL)

    index_block = index_block->next;
  }
}

int
gcode_extrusion_evaluate_offset (gcode_block_t *block, gfloat_t z, gfloat_t *offset)
{
  gcode_block_t *index_block;
  gfloat_t p0[2], p1[2], x_array[2];
  uint32_t x_index;

  index_block = block->listhead;

  while (index_block)
  {
    index_block->ends (index_block, p0, p1, GCODE_GET);

    if ((z >= p0[1] && z <= p1[1]) || (z >= p1[1] && z <= p0[1]))
    {
      x_index = 0;
      index_block->eval (index_block, z, x_array, &x_index);
      *offset = x_array[0];

      return (0);
    }

    index_block = index_block->next;
  }

  return (1);
}

/**
 * Determines if a taper exists, which may imply that pocketing can occur.
 */

int
gcode_extrusion_taper_exists (gcode_block_t *block)
{
  gcode_block_t *index_block;
  gcode_vec2d_t e0, e1, e2;

  index_block = block->listhead;

  if (!index_block)
    return (0);

  index_block->ends (index_block, e0, e1, GCODE_GET);                           /* This will always be a line */

  while (index_block)
  {
    if (index_block->type == GCODE_TYPE_ARC)
      return (1);

    index_block->ends (index_block, e1, e2, GCODE_GET);                         /* This will always be a line */

    if (fabs (e1[0] - e0[0]) > GCODE_PRECISION || fabs (e2[0] - e0[0]) > GCODE_PRECISION)       /* Checking X values */
      return (1);

    index_block = index_block->next;
  }

  return (0);
}
