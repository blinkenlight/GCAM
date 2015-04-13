/**
 *  gcode_drill_holes.c
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
#include "gcode_drill_holes.h"
#include "gcode_point.h"
#include "gcode_tool.h"
#include "gcode.h"

void
gcode_drill_holes_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_drill_holes_t *drill_holes;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_DRILL_HOLES, 0);

  (*block)->free = gcode_drill_holes_free;
  (*block)->save = gcode_drill_holes_save;
  (*block)->load = gcode_drill_holes_load;
  (*block)->make = gcode_drill_holes_make;
  (*block)->draw = gcode_drill_holes_draw;
  (*block)->aabb = gcode_drill_holes_aabb;
  (*block)->move = gcode_drill_holes_move;
  (*block)->spin = gcode_drill_holes_spin;
  (*block)->scale = gcode_drill_holes_scale;
  (*block)->parse = gcode_drill_holes_parse;
  (*block)->clone = gcode_drill_holes_clone;

  (*block)->pdata = malloc (sizeof (gcode_drill_holes_t));

  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Drill Holes");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  drill_holes = (gcode_drill_holes_t *)(*block)->pdata;

  drill_holes->depth = -gcode->material_size[2];
  drill_holes->increment = 0.0;
  drill_holes->optimal_path = 1;

  drill_holes->offset.side = -1.0;
  drill_holes->offset.tool = 0.0;
  drill_holes->offset.eval = 0.0;
  drill_holes->offset.rotation = 0.0;
  drill_holes->offset.origin[0] = 0.0;
  drill_holes->offset.origin[1] = 0.0;
  drill_holes->offset.z[0] = 0.0;
  drill_holes->offset.z[1] = 0.0;

  (*block)->offref = &drill_holes->offset;
}

void
gcode_drill_holes_free (gcode_block_t **block)
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
gcode_drill_holes_make (gcode_block_t *block)
{
  gcode_drill_holes_t *drill_holes;
  gcode_tool_t *tool;
  gcode_block_t *sorted_listhead;
  gcode_block_t *index_block, *index2_block;
  gcode_block_t *best_block, *next_block;
  gcode_vec2d_t p, p2;
  gfloat_t safe_z, touch_z, target_z, z;
  gfloat_t best_dist, dist;
  char string[256];

  GCODE_CLEAR (block);                                                          // Clean up the g-code string of this block to an empty string;

  if (!block->listhead)                                                         // If the list is empty, this will definitely be a quick pass...
    return;

  if (block->flags & GCODE_FLAGS_SUPPRESS)                                      // Same thing if the bolt_holes is currently not supposed to be made;
    return;

  drill_holes = (gcode_drill_holes_t *)block->pdata;                            // Get a reference to the data struct of the drill_holes;

  tool = gcode_tool_find (block);                                               // Find the tool that applies to this block; 

  if (!tool)                                                                    // If there is none, this block will not get made - bail out;
    return;

  sorted_listhead = NULL;

  drill_holes->offset.origin[0] = block->offset->origin[0];                     // Inherit the offset of the parent by copying it into this block's offset;
  drill_holes->offset.origin[1] = block->offset->origin[1];
  drill_holes->offset.rotation = block->offset->rotation;

  safe_z = block->gcode->ztraverse;                                             // This is just a short-hand for the traverse z...

  touch_z = block->gcode->material_origin[2];                                   // Track the depth the material begins at (gets lower after every pass);

  target_z = drill_holes->depth;                                                // Another shorthand for the depth of the holes...

  GCODE_NEWLINE (block);

  sprintf (string, "DRILL HOLES: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  if (block->gcode->drilling_motion == GCODE_DRILLING_CANNED)
  {
    if (drill_holes->increment <= GCODE_PRECISION)                              // Start of peck drilling cycle (G83);
    {
      GCODE_DRILL (block, "G83", target_z, tool->feed * tool->plunge_ratio, safe_z);
    }
    else
    {
      GCODE_Q_DRILL (block, "G83", target_z, tool->feed * tool->plunge_ratio, safe_z, drill_holes->increment);
    }
  }

  index_block = block->listhead;

  if (drill_holes->optimal_path)
  {
    gcode_util_get_sublist_snapshot (&sorted_listhead, index_block, NULL);

    index_block = sorted_listhead;

    while (index_block->next)
    {
      if (index_block->flags & GCODE_FLAGS_SUPPRESS)
      {
        index_block = index_block->next;

        continue;
      }

      gcode_point_with_offset (index_block, p);

      best_block = NULL;

      index2_block = index_block->next;

      while (index2_block)
      {
        if (index2_block->flags & GCODE_FLAGS_SUPPRESS)
        {
          index2_block = index2_block->next;

          continue;
        }

        gcode_point_with_offset (index2_block, p2);

        dist = GCODE_MATH_2D_DISTANCE (p, p2);

        if (dist < GCODE_PRECISION)
        {
          next_block = index2_block->next;

          gcode_remove_and_destroy (index2_block);

          index2_block = next_block;

          continue;
        }

        if ((dist < best_dist) || (!best_block))
        {
          best_dist = dist;

          best_block = index2_block;
        }

        index2_block = index2_block->next;
      }

      if (best_block)
      {
        gcode_place_block_behind (index_block, best_block);
      }

      index_block = index_block->next;
    }

    index_block = sorted_listhead;
  }

  while (index_block)
  {
    if (index_block->flags & GCODE_FLAGS_SUPPRESS)
    {
      index_block = index_block->next;

      continue;
    }

    gcode_point_with_offset (index_block, p);

    if (block->gcode->drilling_motion == GCODE_DRILLING_CANNED)
    {
      GCODE_XY_PAIR (block, p[0], p[1], index_block->comment);
    }
    else
    {
      if (drill_holes->increment < GCODE_PRECISION)
        z = target_z;
      else if (touch_z - target_z > drill_holes->increment)
        z = touch_z - drill_holes->increment;
      else
        z = target_z;

      GCODE_MOVE_TO (block, p[0], p[1], z, safe_z, touch_z, tool, index_block->comment);

      while (z > target_z)
      {
        GCODE_RETRACT (block, safe_z);
        GCODE_PLUMMET (block, 0.95 * z);

        if (z - target_z > drill_holes->increment)
          z = z - drill_holes->increment;
        else
          z = target_z;

        GCODE_DESCEND (block, z, tool);
      }

      GCODE_RETRACT (block, safe_z);
    }

    index_block = index_block->next;
  }

  if (block->gcode->drilling_motion == GCODE_DRILLING_CANNED)
  {
    GCODE_COMMAND (block, "G80", "end canned cycle");                           // End of canned cycle (G80)
    GCODE_F_VALUE (block, tool->feed, "restore feed rate");
  }

  GCODE_RETRACT (block, safe_z);                                                // Pull back up when done;

  gcode_list_free (&sorted_listhead);
}

void
gcode_drill_holes_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_drill_holes_t *drill_holes;
  uint32_t size, marker, num;
  uint8_t data;

  drill_holes = (gcode_drill_holes_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_DRILL_HOLES);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_DRILL_HOLES_DEPTH, drill_holes->depth);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_DRILL_HOLES_INCREMENT, drill_holes->increment);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_DRILL_HOLES_OPTIMAL_PATH, drill_holes->optimal_path);
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
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_DRILL_HOLES);
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

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_DRILL_HOLES_NUMBER, sizeof (uint32_t), &num);

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

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_DRILL_HOLES_DEPTH, sizeof (gfloat_t), &drill_holes->depth);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_DRILL_HOLES_INCREMENT, sizeof (gfloat_t), &drill_holes->increment);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_DRILL_HOLES_OPTIMAL_PATH, sizeof (uint8_t), &drill_holes->optimal_path);
  }
}

void
gcode_drill_holes_load (gcode_block_t *block, FILE *fh)
{
  gcode_drill_holes_t *drill_holes;
  gcode_block_t *new_block;
  uint32_t bsize, dsize, start, num, i;
  uint8_t data, type;

  drill_holes = (gcode_drill_holes_t *)block->pdata;

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

      case GCODE_BIN_DATA_DRILL_HOLES_NUMBER:
        fread (&num, sizeof (uint32_t), 1, fh);

        for (i = 0; i < num; i++)
        {
          /* Read Data */
          fread (&type, sizeof (uint8_t), 1, fh);

          gcode_point_init (&new_block, block->gcode, block);

          gcode_append_as_listtail (block, new_block);                          // Append 'new_block' to the end of 'block's list (as head if the list is NULL)

          new_block->load (new_block, fh);
        }
        break;

      case GCODE_BIN_DATA_DRILL_HOLES_DEPTH:
        fread (&drill_holes->depth, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_DRILL_HOLES_INCREMENT:
        fread (&drill_holes->increment, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_DRILL_HOLES_OPTIMAL_PATH:
        fread (&drill_holes->optimal_path, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_drill_holes_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_drill_holes_t *drill_holes;

  drill_holes = (gcode_drill_holes_t *)block->pdata;

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
    else if (strcmp (name, GCODE_XML_ATTR_DRILL_HOLES_DEPTH) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        drill_holes->depth = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_DRILL_HOLES_INCREMENT) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        drill_holes->increment = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_DRILL_HOLES_OPTIMAL_PATH) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        drill_holes->optimal_path = m;
    }
  }
}

void
gcode_drill_holes_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_drill_holes_t *drill_holes;
  gcode_point_t *point;
  gcode_block_t *index_block;
  gcode_tool_t *tool;
  gfloat_t coef, tool_radius;
  uint32_t sind, i, tess;
  gcode_vec2d_t xform_pt;

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  drill_holes = (gcode_drill_holes_t *)block->pdata;

  tool = gcode_tool_find (block);

  if (tool == NULL)
    return;

  tool_radius = 0.5 * tool->diameter;

  drill_holes->offset.origin[0] = block->offset->origin[0];
  drill_holes->offset.origin[1] = block->offset->origin[1];
  drill_holes->offset.rotation = block->offset->rotation;

  tess = 32;

  index_block = block->listhead;

  while (index_block)
  {
    if (!(index_block->flags & GCODE_FLAGS_SUPPRESS))
    {
      index_block->draw (index_block, NULL);                                    // Draw the point itself

      gcode_point_with_offset (index_block, xform_pt);                          // Now draw the "cylinder" around it - but first find out where it is...

      sind = 0;

      if ((block == selected) || (index_block == selected))
        sind = 1;

      glLoadName ((GLuint) index_block->name);                                  // Set the 'name' of the "cylinder" to match that of the point it surrounds;
      glLineWidth (2);

      glBegin (GL_LINE_STRIP);
      for (i = 0; i < tess; i++)
      {
        coef = (gfloat_t)i / (gfloat_t)(tess - 1);
        glColor3f (GCODE_OPENGL_SELECTABLE_COLORS[sind][0],
                   GCODE_OPENGL_SELECTABLE_COLORS[sind][1],
                   GCODE_OPENGL_SELECTABLE_COLORS[sind][2]);
        glVertex3f (xform_pt[0] + tool_radius * cos (coef * GCODE_2PI),
                    xform_pt[1] + tool_radius * sin (coef * GCODE_2PI),
                    0.0);
      }
      glEnd ();

      glBegin (GL_LINE_STRIP);
      for (i = 0; i < tess; i++)
      {
        coef = (gfloat_t)i / (gfloat_t)(tess - 1);
        glColor3f (GCODE_OPENGL_SELECTABLE_COLORS[sind][0],
                   GCODE_OPENGL_SELECTABLE_COLORS[sind][1],
                   GCODE_OPENGL_SELECTABLE_COLORS[sind][2]);
        glVertex3f (xform_pt[0] + tool_radius * cos (coef * GCODE_2PI),
                    xform_pt[1] + tool_radius * sin (coef * GCODE_2PI),
                    drill_holes->depth);
      }
      glEnd ();

      glBegin (GL_LINES);
      glVertex3f (xform_pt[0] + tool_radius * cos (0.25 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (0.25 * GCODE_2PI),
                  0.0);
      glVertex3f (xform_pt[0] + tool_radius * cos (0.25 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (0.25 * GCODE_2PI),
                  drill_holes->depth);

      glVertex3f (xform_pt[0] + tool_radius * cos (0.50 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (0.50 * GCODE_2PI),
                  0.0);
      glVertex3f (xform_pt[0] + tool_radius * cos (0.50 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (0.50 * GCODE_2PI),
                  drill_holes->depth);

      glVertex3f (xform_pt[0] + tool_radius * cos (0.75 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (0.75 * GCODE_2PI),
                  0.0);
      glVertex3f (xform_pt[0] + tool_radius * cos (0.75 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (0.75 * GCODE_2PI),
                  drill_holes->depth);

      glVertex3f (xform_pt[0] + tool_radius * cos (1.00 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (1.00 * GCODE_2PI),
                  0.0);
      glVertex3f (xform_pt[0] + tool_radius * cos (1.00 * GCODE_2PI),
                  xform_pt[1] + tool_radius * sin (1.00 * GCODE_2PI),
                  drill_holes->depth);
      glEnd ();
    }

    index_block = index_block->next;
  }
#endif
}

/**
 * Construct the axis-aligned bounding box of all the holes in the drill holes;
 * NOTE: this can and does return an "imposible" or "inside-out" bounding box
 * which has its minimum larger than its maximum as a sign of failure to pick
 * up any valid holes, if either the list is empty or none of the members are 
 * points (there should ONLY be points) - THIS SHOULD BE TESTED FOR ON RETURN!
 */

void
gcode_drill_holes_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max)
{
  gcode_block_t *index_block;
  gcode_tool_t *tool;
  gcode_point_t *point;
  gfloat_t radius;

  tool = gcode_tool_find (block);                                               // Find the tool that applies to this block;

  radius = tool ? tool->diameter / 2 : 0;                                       // This should never happen (to a dog - not even to a cowardly one) but hey...

  min[0] = min[1] = 1;                                                          // Confuse the hell out of anyone who's not paying attention properly;
  max[0] = max[1] = 0;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->type == GCODE_TYPE_POINT)                                  // This not being true should never happen either (to a d... oops, sorry);
    {
      point = (gcode_point_t *)index_block->pdata;

      if ((min[0] > max[0]) || (min[1] > max[1]))                               // If bounds were inside-out (unset), accept the hole directly;
      {
        min[0] = point->p[0] - radius;
        max[0] = point->p[0] + radius;
        min[1] = point->p[1] - radius;
        max[1] = point->p[1] + radius;
      }
      else                                                                      // If bounds are already valid (set), look for holes not inside;
      {
        if (point->p[0] - radius < min[0])
          min[0] = point->p[0] - radius;

        if (point->p[0] + radius > max[0])
          max[0] = point->p[0] + radius;

        if (point->p[1] - radius < min[1])
          min[1] = point->p[1] - radius;

        if (point->p[1] + radius > max[1])
          max[1] = point->p[1] + radius;
      }
    }

    index_block = index_block->next;
  }
}

void
gcode_drill_holes_move (gcode_block_t *block, gcode_vec2d_t delta)
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
gcode_drill_holes_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_block_t *index_block;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->spin)
      index_block->spin (index_block, datum, angle);

    index_block = index_block->next;
  }
}

void
gcode_drill_holes_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_drill_holes_t *drill_holes, *model_drill_holes;
  gcode_block_t *index_block, *new_block;

  model_drill_holes = (gcode_drill_holes_t *)model->pdata;

  gcode_drill_holes_init (block, gcode, model->parent);

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  drill_holes = (gcode_drill_holes_t *)(*block)->pdata;

  drill_holes->depth = model_drill_holes->depth;
  drill_holes->increment = model_drill_holes->increment;
  drill_holes->optimal_path = model_drill_holes->optimal_path;

  index_block = model->listhead;

  while (index_block)
  {
    index_block->clone (&new_block, gcode, index_block);

    gcode_append_as_listtail (*block, new_block);                               // Append 'new_block' to the end of 'block's list (as head if the list is NULL)

    index_block = index_block->next;
  }
}

void
gcode_drill_holes_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_drill_holes_t *drill_holes;
  gcode_block_t *index_block;

  drill_holes = (gcode_drill_holes_t *)block->pdata;

  drill_holes->depth *= scale;
  drill_holes->increment *= scale;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->scale)
      index_block->scale (index_block, scale);

    index_block = index_block->next;
  }
}

/**
 * Multiply the elements of a drill holes a number of times, each time applying
 * incremental rotation and translation offset, creating a regularly repeating
 * pattern from the original elements of the block.
 */

void
gcode_drill_holes_pattern (gcode_block_t *block, uint32_t count, gcode_vec2d_t delta, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_block_t *index_block, *final_block, *new_block, *last_block;
  gfloat_t inc_rotation, inc_translate_x, inc_translate_y;
  uint32_t i;

  if (block->listhead)                                                          // No point to stick around if the list is empty.
  {
    final_block = block->listhead;                                              // If we're still here, the list is NOT empty: start from the first block.

    while (final_block->next)                                                   // Crawl along the list until 'final_block' really points to the last block.
      final_block = final_block->next;

    last_block = final_block;                                                   // Point 'last_block' as well to the last block in the list.

    for (i = 1; i < count; i++)                                                 // If 'count' equals 0 or 1, this will do nothing at all - as it should.
    {
      inc_rotation = ((float)i) * angle;                                        // Calculate roto-translation factors for the current iteration
      inc_translate_x = ((float)i) * delta[0];
      inc_translate_y = ((float)i) * delta[1];

      index_block = block->listhead;                                            // Each time, start with the first block in the list (which we know must exist)

      for (;;)                                                                  // Y'all keep coming back until I say otherwise, y'hear?
      {
        gcode_point_t *point, *new_point;
        gcode_vec2d_t xform_pt, pt;

        point = (gcode_point_t *)index_block->pdata;                            // Acquire a reference to the data of the current block;

        gcode_point_init (&new_block, block->gcode, block);                     // Create the new block,
        new_point = (gcode_point_t *)new_block->pdata;                          // and acquire a reference to its data as well.

        /* Rotate and Translate */
        pt[0] = point->p[0] - datum[0];
        pt[1] = point->p[1] - datum[1];
        GCODE_MATH_ROTATE (xform_pt, pt, inc_rotation);
        new_point->p[0] = xform_pt[0] + datum[0] + inc_translate_x;
        new_point->p[1] = xform_pt[1] + datum[1] + inc_translate_y;

        strcpy (new_block->comment, index_block->comment);                      // Copy the comment string of the current block to the new one;

        gcode_insert_after_block (last_block, new_block);                       // Attach the new block after the current last block in the list,

        last_block = new_block;                                                 // and designate it as the new last block in the list;

        if (index_block != final_block)                                         // If the current block is not the one that was the last in the original list
          index_block = index_block->next;                                      // (as referenced by 'final_block'), keep crawling along.
        else                                                                    // If the current block is the one that used to be the last one originally,
          break;                                                                // Y'all may now go! Y'all be coming right back for the next iteration anyways.
      }
    }
  }
}
