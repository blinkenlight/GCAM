/**
 *  gcode_template.c
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

#include "gui_define.h"
#include "gcode_template.h"
#include "gcode.h"

void
gcode_template_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_template_t *template;

  *block = (gcode_block_t *)malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_TEMPLATE, 0);

  (*block)->free = gcode_template_free;
  (*block)->make = gcode_template_make;
  (*block)->save = gcode_template_save;
  (*block)->load = gcode_template_load;
  (*block)->draw = gcode_template_draw;
  (*block)->clone = gcode_template_clone;
  (*block)->scale = gcode_template_scale;
  (*block)->parse = gcode_template_parse;

  (*block)->pdata = malloc (sizeof (gcode_template_t));

  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Template");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  template = (gcode_template_t *)(*block)->pdata;

  template->position[0] = 0.0;
  template->position[1] = 0.0;
  template->rotation = 0.0;

  template->offset.side = 0.0;
  template->offset.tool = 0.0;
  template->offset.eval = 0.0;
  template->offset.rotation = 0.0;
  template->offset.origin[0] = 0.0;
  template->offset.origin[1] = 0.0;
  template->offset.z[0] = 0.0;
  template->offset.z[1] = 0.0;

  (*block)->offref = &template->offset;
}

void
gcode_template_free (gcode_block_t **block)
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
gcode_template_make (gcode_block_t *block)
{
  gcode_template_t *template;
  gcode_block_t *index_block;
  gcode_vec2d_t xform_origin;
  char string[256];

  template = (gcode_template_t *)block->pdata;

  GCODE_CLEAR (block);

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  // Inherit whatever offset this block acquires from its parent, if any,
  // and compound it with whatever offset this block generates on its own

  GCODE_MATH_VEC2D_SET (xform_origin, template->position[0], template->position[1]);
  GCODE_MATH_ROTATE (xform_origin, xform_origin, block->offset->rotation);
  GCODE_MATH_TRANSLATE (xform_origin, xform_origin, block->offset->origin);

  template->offset.origin[0] = xform_origin[0];
  template->offset.origin[1] = xform_origin[1];
  template->offset.rotation = fmod (block->offset->rotation + template->rotation, 360.0);

  GCODE_APPEND (block, "\n");

  sprintf (string, "TEMPLATE: %s", block->comment);
  GCODE_COMMENT (block, string);

  index_block = block->listhead;

  while (index_block)
  {
    index_block->make (index_block);

    GCODE_APPEND (block, index_block->code);

    index_block = index_block->next;
  }
}

void
gcode_template_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_template_t *template;
  uint32_t size, num, marker;
  uint8_t data;

  template = (gcode_template_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_TEMPLATE);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_2D_FLT (fh, GCODE_XML_ATTR_TEMPLATE_POSITION, template->position);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_TEMPLATE_ROTATION, template->rotation);
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
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_TEMPLATE);
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

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TEMPLATE_NUMBER, sizeof (uint32_t), &num);

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

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TEMPLATE_POSITION, 2 * sizeof (gfloat_t), template->position);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_TEMPLATE_ROTATION, sizeof (gfloat_t), &template->rotation);
  }
}

void
gcode_template_load (gcode_block_t *block, FILE *fh)
{
  gcode_template_t *template;
  gcode_block_t *new_block;
  uint32_t bsize, dsize, start, num, i;
  uint8_t data, type;

  template = (gcode_template_t *)block->pdata;
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

      case GCODE_BIN_DATA_TEMPLATE_NUMBER:
        fread (&num, sizeof (uint32_t), 1, fh);

        for (i = 0; i < num; i++)
        {
          /* Read Data */
          fread (&type, sizeof (uint8_t), 1, fh);

          switch (type)
          {
            case GCODE_TYPE_TOOL:
              gcode_tool_init (&new_block, block->gcode, block);
              break;

            case GCODE_TYPE_TEMPLATE:
              gcode_template_init (&new_block, block->gcode, block);
              break;

            case GCODE_TYPE_SKETCH:
              gcode_sketch_init (&new_block, block->gcode, block);
              break;

            case GCODE_TYPE_BOLT_HOLES:
              gcode_bolt_holes_init (&new_block, block->gcode, block);
              break;

            case GCODE_TYPE_DRILL_HOLES:
              gcode_drill_holes_init (&new_block, block->gcode, block);
              break;

            default:
              break;
          }

          gcode_append_as_listtail (block, new_block);                          // Append 'new_block' to the end of 'block's list (as head if the list is NULL)

          new_block->load (new_block, fh);
        }
        break;

      case GCODE_BIN_DATA_TEMPLATE_POSITION:
        fread (template->position, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_TEMPLATE_ROTATION:
        fread (&template->rotation, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_template_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_template_t *template;

  template = (gcode_template_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_TEMPLATE_POSITION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_FLT (xyz, value))
        for (int j = 0; j < 2; j++)
          template->position[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_TEMPLATE_ROTATION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        template->rotation = (gfloat_t)w;
    }
  }
}

void
gcode_template_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_template_t *template;
  gcode_block_t *index_block;
  gcode_vec2d_t xform_origin;

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  template = (gcode_template_t *)block->pdata;

  // Inherit whatever offset this block acquires from its parent, if any,
  // and compound it with whatever offset this block generates on its own

  GCODE_MATH_VEC2D_SET (xform_origin, template->position[0], template->position[1]);
  GCODE_MATH_ROTATE (xform_origin, xform_origin, block->offset->rotation);
  GCODE_MATH_TRANSLATE (xform_origin, xform_origin, block->offset->origin);

  template->offset.origin[0] = xform_origin[0];
  template->offset.origin[1] = xform_origin[1];
  template->offset.rotation = fmod (block->offset->rotation + template->rotation, 360.0);

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->draw)
    {
      if (selected == block)
      {
        index_block->draw (index_block, index_block);
      }
      else
      {
        index_block->draw (index_block, selected);
      }
    }

    index_block = index_block->next;
  }

  glPointSize (GCODE_OPENGL_DATUM_POINT_SIZE);
  glColor3f (GCODE_OPENGL_DATUM_POINT_COLOR[0],
             GCODE_OPENGL_DATUM_POINT_COLOR[1],
             GCODE_OPENGL_DATUM_POINT_COLOR[2]);
  glBegin (GL_POINTS);
  glVertex3f (template->offset.origin[0], template->offset.origin[1], 0.0);
  glEnd ();
#endif
}

void
gcode_template_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_template_t *template, *model_template;
  gcode_block_t *index_block, *new_block;

  model_template = (gcode_template_t *)model->pdata;

  gcode_template_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  template = (gcode_template_t *)(*block)->pdata;

  template->position[0] = model_template->position[0];
  template->position[1] = model_template->position[1];
  template->rotation = model_template->rotation;
  template->offset = model_template->offset;

  index_block = model->listhead;

  while (index_block)
  {
    index_block->clone (&new_block, gcode, index_block);

    gcode_append_as_listtail (*block, new_block);                               // Append 'new_block' to the end of 'block's list (as head if the list is NULL)

    index_block = index_block->next;
  }
}

void
gcode_template_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_template_t *template;
  gcode_block_t *index_block;

  template = (gcode_template_t *)block->pdata;

  template->position[0] *= scale;
  template->position[1] *= scale;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->scale)                                                     /* Because a Tool could be part of this list */
      index_block->scale (index_block, scale);

    index_block = index_block->next;
  }
}
