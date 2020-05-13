/**
 *  gcode_template.c
 *  Source code file for G-Code generation, simulation, and visualization
 *  library.
 *
 *  Copyright (C) 2006 - 2010 by Justin Shumaker
 *  Copyright (C) 2014 - 2020 by Asztalos Attila Oszkár
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

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_TEMPLATE, 0);

  (*block)->free = gcode_template_free;
  (*block)->save = gcode_template_save;
  (*block)->load = gcode_template_load;
  (*block)->make = gcode_template_make;
  (*block)->draw = gcode_template_draw;
  (*block)->aabb = gcode_template_aabb;
  (*block)->move = gcode_template_move;
  (*block)->spin = gcode_template_spin;
  (*block)->flip = gcode_template_flip;
  (*block)->scale = gcode_template_scale;
  (*block)->parse = gcode_template_parse;
  (*block)->clone = gcode_template_clone;

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
  template->offset.rotation = block->offset->rotation + template->rotation;

  GCODE_MATH_WRAP_TO_360_DEGREES (template->offset.rotation);

  GCODE_NEWLINE (block);

  sprintf (string, "TEMPLATE: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  index_block = block->listhead;

  while (index_block)
  {
    index_block->make (index_block);

    GCODE_APPEND (block, index_block->code);

    index_block = index_block->next;
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
  template->offset.rotation = block->offset->rotation + template->rotation;

  GCODE_MATH_WRAP_TO_360_DEGREES (template->offset.rotation);

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
gcode_template_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max, uint8_t mode)
{
  gcode_block_t *index_block;
  gcode_vec2d_t tmin, tmax;

  index_block = block->listhead;

  min[0] = min[1] = 1;                                                          // Never cross the streams, you say...? Oh well, too late...
  max[0] = max[1] = 0;                                                          // Callers should test for an inside-out aabb being returned;

  if ((mode != GCODE_GET) && (mode != GCODE_GET_WITH_OFFSET))
    return;

  while (index_block)
  {
    if (!index_block->aabb)                                                     // If the block has no bounds function, don't try to call it;
    {
      index_block = index_block->next;
      continue;
    }

    index_block->aabb (index_block, tmin, tmax, mode);

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
    else
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

  /**
   * The "relative" no-offset mode is useful for working with basic primitives
   * like arcs, lines etc. but it becomes meaningless as soon as templates get
   * involved. Merging the un-offset AABBs of objects displaced by templates at
   * various nesting levels yields no meaningful result. Therefore, templates
   * make no attempt to adjust AABBs retrieved in WITH_OFFSET mode, but adjust
   * those AABBs with their own displacement in no-offset mode; this keeps them
   * comparable even across nesting levels, always relative to the local origin
   */

  if ((mode == GCODE_GET) && (min[0] < max[0]) && (min[1] < max[1]))
  {
    gcode_template_t *template;

    template = (gcode_template_t *)block->pdata;

    GCODE_MATH_ROTATE (min, min, template->rotation);
    GCODE_MATH_TRANSLATE (min, min, template->position);

    GCODE_MATH_ROTATE (max, max, template->rotation);
    GCODE_MATH_TRANSLATE (max, max, template->position);

    if (min[0] > max[0])
      GCODE_MATH_SWAP(min[0], max[0]);

    if (min[1] > max[1])
      GCODE_MATH_SWAP(min[1], max[1]);
  }
}

void
gcode_template_move (gcode_block_t *block, gcode_vec2d_t delta)
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
gcode_template_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_block_t *index_block;
  gcode_template_t *template;
  gcode_vec2d_t reverse_shift;
  gfloat_t reverse_spin;

  template = (gcode_template_t *)block->pdata;

  reverse_shift[0] = -template->position[0];
  reverse_shift[1] = -template->position[1];
  reverse_spin = -template->rotation;

  GCODE_MATH_WRAP_TO_360_DEGREES (reverse_spin);

  GCODE_MATH_TRANSLATE (datum, datum, reverse_shift);
  GCODE_MATH_ROTATE (datum, datum, reverse_spin);

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->spin)
      index_block->spin (index_block, datum, angle);

    index_block = index_block->next;
  }
}

void
gcode_template_flip (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_template_t *template;
  gcode_block_t *index_block;
  gcode_vec2d_t origin;

  template = (gcode_template_t *)block->pdata;

  if (GCODE_MATH_IS_EQUAL (angle, 0))
  {
    template->position[1] -= datum[1];
    template->position[1] = -template->position[1];
    template->position[1] += datum[1];

    template->rotation = 360 - template->rotation;                              // The reverse of the usual angle due to further mirroring inside the template
    GCODE_MATH_WRAP_TO_360_DEGREES(template->rotation);

  }

  if (GCODE_MATH_IS_EQUAL (angle, 90))
  {
    template->position[0] -= datum[0];
    template->position[0] = -template->position[0];
    template->position[0] += datum[0];

    template->rotation = 360 - template->rotation;                              // The reverse of the usual angle due to further mirroring inside the template
    GCODE_MATH_WRAP_TO_360_DEGREES(template->rotation);
  }

  GCODE_MATH_VEC2D_SET (origin, 0, 0);

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->flip)
      index_block->flip (index_block, origin, angle);

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
