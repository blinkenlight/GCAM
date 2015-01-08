/**
 *  gcode.c
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

#include "gcode.h"
#include <stdio.h>
#include <float.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <locale.h>
#include <expat.h>
#include "gcode_util.h"
#include "gcode_sim.h"

/**
 * Attaches 'block' as the first (and only) element of 'sel_block's' extruder;
 */

void
gcode_attach_as_extruder (gcode_block_t *sel_block, gcode_block_t *block)
{
  if (sel_block && block)                                                       // If 'sel_block' or 'block' is NULL, there's not much to do here is it...
  {
    block->prev = NULL;                                                         // An element attached as extruder is not supposed to have horizontal peers;
    block->next = NULL;
    block->parent = sel_block;                                                  // It does however definitely have a parent;
    block->offset = sel_block->offref;                                          // This might be useless, but let's do it anyway: hook up 'offset' to the parent
                                                                                // ('offref' points to a block's OWN offset member or the gcode's 'zero_offset');
    sel_block->extruder = block;                                                // Finally do what we came here for: attach 'block' to the parent's extruder.
  }
}

/**
 * Inserts 'block' as the first element of 'sel_block's' list (as the listhead);
 * If 'sel_block' is NULL that is taken to mean 'no current block', so 'block'
 * gets inserted into his own 'gcode's' list instead. In both cases, if the list
 * is empty (the listhead is NULL), 'block' simply becomes the new listhead.
 */

void
gcode_insert_as_listhead (gcode_block_t *sel_block, gcode_block_t *block)
{
  if (block)                                                                    // If 'block' is NULL, there's not much to do here is it...
  {
    block->prev = NULL;                                                         // An element inserted as listhead is not supposed to have a 'prev' peer;
    block->parent = sel_block;                                                  // It does however definitely have a parent (even if it turns out to be NULL);

    if (sel_block)                                                              // A request with a NULL parent implies attachment directly under the gcode;
    {
      block->next = sel_block->listhead;                                        // Register the current listhead of 'sel_block' as the new 'next' of 'block';

      if (block->next)                                                          // If the current listhead is not NULL,
        block->next->prev = block;                                              // register 'block' as its new 'prev';

      sel_block->listhead = block;                                              // Register 'block' as the parent's new listhead;

      block->offset = sel_block->offref;                                        // Set the 'offset' reference to the parent's 'offref'.
    }
    else                                                                        // If the parent is the gcode itself,
    {
      block->next = block->gcode->listhead;                                     // register the current listhead of 'gcode' as the new 'next' of 'block';

      if (block->next)                                                          // If the current listhead is not NULL,
        block->next->prev = block;                                              // register 'block' as its new 'prev';

      block->gcode->listhead = block;                                           // Register 'block' as the parent's new listhead;

      block->offset = &block->gcode->zero_offset;                               // Set the 'offset' reference to the 'zero_offset' member of the parent gcode.
    }
  }
}

/**
 * Appends 'block' as the last element of 'sel_block's' list (as the listtail);
 * If 'sel_block' is NULL that is taken to mean 'no current block', so 'block'
 * gets appended to his own 'gcode's' list instead. In both cases, if the list
 * is empty (the listhead is NULL), 'block' simply becomes the new listhead.
 */

void
gcode_append_as_listtail (gcode_block_t *sel_block, gcode_block_t *block)
{
  gcode_block_t *last_block;

  if (block)                                                                    // If 'block' is NULL, there's not much to do here is it...
  {
    block->next = NULL;                                                         // An element APPENDED as listtail is not supposed to have a 'next' peer;
    block->parent = sel_block;                                                  // It does however definitely have a parent (even if it turns out to be NULL);

    if (sel_block)                                                              // A request with a NULL parent implies attachment directly under the gcode;
    {
      last_block = sel_block->listhead;                                         // If 'sel_block' exists, acquire a reference to its current listhead;

      if (last_block)                                                           // If it is not NULL, we need to find the last element attached after it,
      {
        while (last_block->next)                                                // so keep crawling along the list as long as 'next' is not NULL.
          last_block = last_block->next;

        block->prev = last_block;                                               // Once we have the last block, register it as preceding 'block' (its 'prev'),
        last_block->next = block;                                               // then register 'block' as now succeeding the former last block (its 'next');
      }
      else                                                                      // If the listhead turns out to be NULL,
      {
        block->prev = NULL;                                                     // acknowledge that there will be no blocks preceding 'block',
        sel_block->listhead = block;                                            // then attach 'block' directly to the parent's listhead.
      }

      block->offset = sel_block->offref;                                        // If the parent exists, set the 'offset' reference to the parent's 'offref'.
    }
    else                                                                        // If the parent is the gcode itself,
    {
      last_block = block->gcode->listhead;                                      // acquire a reference to the current listhead of 'gcode';

      if (last_block)                                                           // If it is not NULL, we need to find the last element attached after it,
      {
        while (last_block->next)                                                // so keep crawling along the list as long as 'next' is not NULL.
          last_block = last_block->next;

        block->prev = last_block;                                               // Once we have the last block, register it as preceding 'block' (its 'prev'),
        last_block->next = block;                                               // then register 'block' as now succeeding the former last block (its 'next');
      }
      else                                                                      // If the listhead turns out to be NULL,
      {
        block->prev = NULL;                                                     // acknowledge that there will be no blocks preceding 'block',
        block->gcode->listhead = block;                                         // then attach 'block' directly to the parent's listhead.
      }

      block->offset = &block->gcode->zero_offset;                               // Set the 'offset' reference to the 'zero_offset' member of the parent gcode.
    }
  }
}

/**
 * Inserts 'block' after (as the 'next' element of) 'sel_block'; if other
 * blocks already existed as 'next', they get re-attached after 'block';
 */

void
gcode_insert_after_block (gcode_block_t *sel_block, gcode_block_t *block)
{
  if (sel_block && block)                                                       // If 'sel_block' or 'block' is NULL, there's not much to do here is it...
  {
    block->prev = sel_block;                                                    // Register 'sel_block' as the previous peer of 'block';
    block->next = sel_block->next;                                              // Register the current next peer of 'sel_block' as the next peer of 'block';
    block->parent = sel_block->parent;                                          // Copy the 'parent' reference directly from 'sel_block';
    block->offset = sel_block->offset;                                          // Copy the 'offset' reference directly from 'sel_block';

    sel_block->next = block;                                                    // Finally do what we came here for: attach 'block' as next peer of 'sel_block',

    if (block->next)                                                            // and if 'block' does have a next peer, register 'block' as its new prev peer.
      block->next->prev = block;
  }
}

/**
 * Re-positions 'block' before (as the 'prev' element of) 'sel_block' within a
 * single list (meaning both blocks must already belong to the same list); the
 * original neighbours of 'block' get spliced back together, but features like
 * 'parent' or 'offset' are assumed to already be correct and are not modified,
 * and the parent's listhead is not updated either - if 'block' used to be the
 * listhead before the repositioning, it will still be the listhead after it;
 */

void
gcode_place_block_before (gcode_block_t *sel_block, gcode_block_t *block)
{
  if (!sel_block || !block)                                                     // If 'sel_block' or 'block' is NULL, there's nothing to do here...
    return;

  if (sel_block->parent != block->parent)                                       // If 'sel_block' and 'block' have different parents, it's a no go;
    return;

  if (block->flags & GCODE_FLAGS_LOCK)                                          // Locked blocks should not be moved around;
    return;

  if (block->next)                                                              // If 'block' had a next peer, link it to whatever 'block' has as 'prev';
    block->next->prev = block->prev;

  if (block->prev)                                                              // If 'block' had a prev peer, link it to whatever 'block' has as 'next';
    block->prev->next = block->next;

  block->next = sel_block;                                                      // Register 'sel_block' as the next peer of 'block';
  block->prev = sel_block->prev;                                                // Register the current prev peer of 'sel_block' as the prev peer of 'block';

  block->next->prev = block;                                                    // Finally do what we came here for: attach 'block' as prev peer of 'sel_block',

  if (block->prev)                                                              // and if 'block' does have a prev peer, register 'block' as its new next peer.
    block->prev->next = block;
}

/**
 * Re-positions 'block' after (as the 'next' element of) 'sel_block' within a
 * single list (meaning both blocks must already belong to the same list); the
 * original neighbours of 'block' get spliced back together, but features like
 * 'parent' or 'offset' are assumed to already be correct and are not modified,
 * and the parent's listhead is not updated either - if 'block' used to be the
 * listhead before the repositioning, it will still be the listhead after it;
 */

void
gcode_place_block_behind (gcode_block_t *sel_block, gcode_block_t *block)
{
  if (!sel_block || !block)                                                     // If 'sel_block' or 'block' is NULL, there's nothing to do here...
    return;

  if (sel_block->parent != block->parent)                                       // If 'sel_block' and 'block' have different parents, it's a no go;
    return;

  if (block->flags & GCODE_FLAGS_LOCK)                                          // Locked blocks should not be moved around;
    return;

  if (block->next)                                                              // If 'block' had a next peer, link it to whatever 'block' has as 'prev';
    block->next->prev = block->prev;

  if (block->prev)                                                              // If 'block' had a previous peer, link it to whatever 'block' has as 'next';
    block->prev->next = block->next;

  block->prev = sel_block;                                                      // Register 'sel_block' as the previous peer of 'block';
  block->next = sel_block->next;                                                // Register the current next peer of 'sel_block' as the next peer of 'block';

  block->prev->next = block;                                                    // Finally do what we came here for: attach 'block' as next peer of 'sel_block',

  if (block->next)                                                              // and if 'block' does have a next peer, register 'block' as its new prev peer.
    block->next->prev = block;
}

/**
 * Removes 'block' from the list it was part of (if any), and reconnects
 * the surrounding blocks (if any) to each other, splicing what remains;
 */

void
gcode_splice_list_around (gcode_block_t *block)
{
  if (block->flags & GCODE_FLAGS_LOCK)                                          // Do not remove a locked block
    return;

  if (block->next)                                                              // If 'block' had a next peer, link it to whatever 'block' has as 'prev';
    block->next->prev = block->prev;

  if (block->prev)                                                              // If 'block' had a previous peer, link it to whatever 'block' has as 'next';
    block->prev->next = block->next;

  if (block->parent)                                                            // If 'block's parent is not NULL,
  {
    if (block->parent->listhead == block)                                       // and 'block' happens to be its listhead,
      block->parent->listhead = block->next;                                    // update that listhead to whatever comes after 'block';
  }
  else                                                                          // If 'block' has no other block as parent,
  {
    if (block->gcode->listhead == block)                                        // but it is the listhead of its own gcode,
      block->gcode->listhead = block->next;                                     // update that listhead to whatever comes after 'block';
  }

  block->prev = NULL;                                                           // Unlink 'block' from everything else.
  block->next = NULL;
  block->parent = NULL;
}

/**
 * Removes 'block' from the list it was part of (if any), reconnects the
 * surrounding blocks (if any) to each other splicing what remains, then
 * destroys the block and any children it might have had (free its list);
 */

void
gcode_remove_and_destroy (gcode_block_t *block)
{
  gcode_splice_list_around (block);
  block->free (&block);
}

void
gcode_get_furthest_prev (gcode_block_t **block)
{
  if (!(*block))
    return;

  while ((*block)->prev)
    *block = (*block)->prev;
}

void
gcode_get_furthest_next (gcode_block_t **block)
{
  if (!(*block))
    return;

  while ((*block)->next)
    *block = (*block)->next;
}

void
gcode_get_circular_prev (gcode_block_t **block)
{
  if (!(*block))
    return;

  if ((*block)->prev)
    *block = (*block)->prev;
  else
    gcode_get_furthest_next (block);
}

void
gcode_get_circular_next (gcode_block_t **block)
{
  if (!(*block))
    return;

  if ((*block)->next)
    *block = (*block)->next;
  else
    gcode_get_furthest_prev (block);
}

void
gcode_list_insert (gcode_block_t **list, gcode_block_t *block)
{
  if (*list)
  {
    gcode_block_t *tmp;

    if ((*list)->next)
    {
      tmp = (*list)->next;

      (*list)->next = block;
      block->prev = *list;
      block->next = tmp;

      tmp->prev = block;
    }
    else
    {
      (*list)->next = block;
      block->prev = *list;
      block->next = NULL;
    }
  }
  else
  {
    *list = block;
    (*list)->prev = NULL;
    (*list)->next = NULL;
  }
}

void
gcode_list_make (gcode_t *gcode)
{
  gcode_block_t *index_block;
  int i, num;

  num = 0;
  index_block = gcode->listhead;

  while (index_block)
  {
    num++;

    index_block = index_block->next;
  }

  /**
   * This can only be run after the list prev/next pointers are
   * set up, hence this is not done as each block is loaded.
   */

  gcode->tool_xpos = FLT_MAX;
  gcode->tool_ypos = FLT_MAX;
  gcode->tool_zpos = FLT_MAX;

  i = 0;
  index_block = gcode->listhead;

  while (index_block)
  {
    /* Make the G-Code */
    index_block->make (index_block);

    if (gcode->progress_callback)
      gcode->progress_callback (gcode->gui, (gfloat_t)i / (gfloat_t)num);

    i++;

    index_block = index_block->next;
  }

  if (gcode->progress_callback)
    gcode->progress_callback (gcode->gui, 1.0);
}

void
gcode_list_free (gcode_block_t **list)
{
  gcode_block_t *tmp;

  /* Walk the list and free */
  while (*list)
  {
    tmp = *list;
    *list = (*list)->next;
    tmp->free (&tmp);
  }

  *list = NULL;
}

static xml_context_t *
gcode_xml_create_context (gcode_t *gcode)
{
  xml_context_t *context = malloc (sizeof (xml_context_t));

  context->gcode = gcode;
  context->block = NULL;                                                        // Points to the current (last inserted) block to which new blocks should be attached.
  context->error = 1;                                                           // Certain conditions have to be met for success - if not, the default result is error.
  context->state = 0;                                                           // This is where said conditions can be accumulated, to be tested at the final end tag.
  context->chars = 0;                                                           // Number of 'character data' chars stored temporarily in the context cache buffer.
  context->index = 0;                                                           // Number of 'character data' items parsed so far inside the current 'image' element.
  context->modus = GCODE_XML_ATTACH_UNDER;                                      // Selects attachment point for a new block: either under or after the current one.

  return (context);
}

static void
gcode_parse (gcode_t *gcode, const char **xmlattr)
{
  for (int i = 0; xmlattr[i]; i += 2)
  {
    int m;
    unsigned int n;
    double xyz[3], w;
    const char *name, *value;

    name = xmlattr[i];
    value = xmlattr[i + 1];

    if (strcmp (name, GCODE_XML_ATTR_GCODE_NAME) == 0)
    {
      GCODE_PARSE_XML_ATTR_STRING (gcode->name, value);
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_UNITS) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        gcode->units = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_MATERIAL_TYPE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        gcode->material_type = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_MATERIAL_SIZE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_3D_FLT (xyz, value))
        for (int j = 0; j < 3; j++)
          gcode->material_size[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_MATERIAL_ORIGIN) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_3D_FLT (xyz, value))
        for (int j = 0; j < 3; j++)
          gcode->material_origin[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_Z_TRAVERSE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        gcode->ztraverse = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_NOTES) == 0)
    {
      GCODE_PARSE_XML_ATTR_STRING (gcode->notes, value);
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_MACHINE_NAME) == 0)
    {
      GCODE_PARSE_XML_ATTR_STRING (gcode->machine_name, value);
    }
    else if (strcmp (name, GCODE_XML_ATTR_GCODE_MACHINE_OPTIONS) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_AS_HEX (n, value))
        gcode->machine_options = n;
    }
  }
}

static void
gcode_xml_char (void *parser, const char *data, int length)
{
  gfloat_t *array;
  char *stage, *scan1, *scan2, *scan3;

  gcode_image_t *image;

  xml_context_t *context = (xml_context_t *) XML_GetUserData (parser);          // Retrieve the context via the supplied parser reference;

  if (context->block)                                                           // The current block has to be the 'image' we work on,
  {
    if (context->block->type == GCODE_TYPE_IMAGE)                               // but we should make sure anyway - we need to dereference it;
    {
      image = (gcode_image_t *)context->block->pdata;                           // Acquire a reference to the 'image'-specific data section;

      array = (gfloat_t *)image->dmap;                                          // Retrieve a reference to the depth map of the 'image' block;

      stage = malloc (context->chars + length + 1);                             // Allocate an appropriately larger staging buffer;

      if (stage)                                                                // If the buffer was obtained, start filling it up;
      {
        memcpy (stage, context->cache, context->chars);                         // First, insert the content of the cache from the last run,
        memcpy (stage + context->chars, data, length);                          // then whatever data the XML parser just supplied this time,
        memset (stage + context->chars + length, '\0', 1);                      // then a terminating zero (the parser data is NOT a string!)

        scan1 = scan2 = scan3 = stage;                                          // Reset all references to the start of the staging buffer;

        do                                                                      // Start traversing the staging buffer;
        {
          if (context->index < context->limit)                                  // Avoid going past the size allocated for the depth map;
          {
            scan3 = scan2;                                                      // We need a two-level history of the scanning pointer:
            scan2 = scan1;                                                      // scan1 = current, scan2 = previous, scan3 = before previous.

            array[context->index] = strtod (scan1, &scan1);                     // Find one float-type number and store it into the depth map;

            context->index++;                                                   // Increment the array index.
          }
          else                                                                  // If the depth map is full and data is still coming in,
          {
            break;                                                              // break out (this should not happen in a properly saved file).
          }
        } while (scan1 != scan2);                                               // Keep looping until the pointers 'pile up' at the buffer's end;

        context->chars += length - ((intptr_t)scan3 - (intptr_t)stage);         // Ok, just trust me - this yields the number of chars to be cached for the next run
                                                                                // (the last thing parsed is still referenced by scan3 as scan1 and scan2 'pile up');
        if (context->chars < sizeof (context->cache))                           // If we broke out that number might be quite BIG - the cache IS NOT.
        {
          memcpy (context->cache, scan3, context->chars);                       // Take the last-parsed buffer fragment and copy it into the cache;

          context->index = context->index > 2 ? context->index - 2 : 0;         // Since we shall re-parse the cache next time, back up the index;
        }
        else                                                                    // Ouch! Cache overflow, likely due to breaking out - just give up:
        {
          context->chars = 0;                                                   // at the very least, reset the cache to zero then hope for the best.
        }

        free (stage);                                                           // Free the staging buffer - next time we'll need a different size.
      }
      else                                                                      // If we got no staging buffer, that's baaaad, 'mkay?
      {
        REMARK ("Failed to allocate memory for stage buffer in XML parser\n");
      }
    }
  }
}

static void
gcode_xml_start (void *parser, const char *tag, const char **attr)
{
  gcode_block_t *index_block, *new_block = NULL;

  xml_context_t *context = (xml_context_t *) XML_GetUserData (parser);          // Retrieve the context via the supplied parser reference;

  if (strcmp (tag, GCODE_XML_TAG_PROJECT) == 0)                                 // 'PROJECT' start tag found...
  {
    context->state |= GCODE_XML_FLAG_PROJECT;                                   // ...so tick the relevant 'checkbox': project? check!
  }
  else if (strcmp (tag, GCODE_XML_TAG_GCODE) == 0)                              // 'GCODE' start tag found...
  {
    if (context->state & GCODE_XML_FLAG_PROJECT)                                // ...but it is only valid if a project exists;
    {
      if (context->gcode->listhead)                                             // If the current listhead is not NULL,
        gcode_list_free (&context->gcode->listhead);                            // it bloody well should be: unroll it.

      gcode_parse (context->gcode, attr);                                       // Restore gcode data from xml attribute list

      context->state |= GCODE_XML_FLAG_GCODE;                                   // Tick the relevant 'checkbox': gcode? check!
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_BEGIN) == 0)                              // 'BEGIN' start tag found...
  {
    if (context->state & GCODE_XML_FLAG_GCODE)                                  // ...but it is only valid if a gcode exists;
    {
      if (context->gcode->listhead)                                             // If the current listhead is not NULL,
        gcode_list_free (&context->gcode->listhead);                            // it bloody well should be: unroll it.

      gcode_begin_init (&new_block, context->gcode, NULL);                      // Create a new 'begin' block,

      if (new_block)                                                            // and if it actually exists,
      {
        gcode_insert_as_listhead (NULL, new_block);                             // connect it directly to the gcode's listhead.

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list

        context->state |= GCODE_XML_FLAG_BEGIN;                                 // Tick the relevant 'checkbox': begin? check!
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_END) == 0)                                // 'END' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      if (context->block->parent == NULL)                                       // Also, it is only valid if this is the top level;
      {
        gcode_end_init (&new_block, context->gcode, NULL);                      // Create a new 'end' block,

        if (new_block)                                                          // and if it actually exists,
        {
          gcode_insert_after_block (context->block, new_block);                 // attach it after the current block

          new_block->parse (new_block, attr);                                   // Restore block data from xml attribute list

          context->state |= GCODE_XML_FLAG_END;                                 // tick the relevant 'checkbox': end? check!
        }
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_TOOL) == 0)                               // 'TOOL' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_tool_init (&new_block, context->gcode, context->block->parent);     // Create a new 'end' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_TEMPLATE) == 0)                           // 'TEMPLATE' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_template_init (&new_block, context->gcode, context->block->parent); // Create a new 'template' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_SKETCH) == 0)                             // 'SKETCH' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_sketch_init (&new_block, context->gcode, context->block->parent);   // Create a new 'sketch' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_DRILL_HOLES) == 0)                        // 'DRILL HOLES' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_drill_holes_init (&new_block, context->gcode, context->block->parent);      // Create a new 'drill holes' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_BOLT_HOLES) == 0)                         // 'BOLT HOLES' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_bolt_holes_init (&new_block, context->gcode, context->block->parent);       // Create a new 'bolt holes' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_EXTRUSION) == 0)                          // 'EXTRUSION' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_extrusion_init (&new_block, context->gcode, context->block->parent);        // Create a new 'extrusion' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (new_block->listhead)                                                // right after doing a casual sanity check
          gcode_list_free (&new_block->listhead);                               // and scrubbing the pre-initialized default 'line',

        gcode_attach_as_extruder (context->block, new_block);                   // attach it UNDER the current block, as its extruder

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_LINE) == 0)                               // 'LINE' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_line_init (&new_block, context->gcode, context->block->parent);     // Create a new 'line' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_ARC) == 0)                                // 'ARC' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_arc_init (&new_block, context->gcode, context->block->parent);      // Create a new 'arc' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_POINT) == 0)                              // 'POINT' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_point_init (&new_block, context->gcode, context->block->parent);    // Create a new 'point' block,

      if (new_block)                                                            // and if it actually exists,
      {
        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list
      }
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_IMAGE) == 0)                              // 'IMAGE' start tag found...
  {
    if ((context->state & GCODE_XML_FLAG_BEGIN) && context->block)              // ...but it is only valid if a begin exists;
    {
      gcode_image_init (&new_block, context->gcode, context->block->parent);    // Create a new 'image' block,

      if (new_block)                                                            // and if it actually exists,
      {
        gcode_image_t *image;

        if (context->modus == GCODE_XML_ATTACH_UNDER)                           // depending on what the current attach modus is,
          gcode_append_as_listtail (context->block, new_block);                 // attach it UNDER the current block, or
        else
          gcode_insert_after_block (context->block, new_block);                 // attach it AFTER the current block;

        new_block->parse (new_block, attr);                                     // Restore block data from xml attribute list

        image = (gcode_image_t *)new_block->pdata;                              // Acquire a pointer to the 'image'-specific data;

        if (image->dmap)                                                        // If the pixel data array exists (resolution is not 0x0),
        {
          context->index = 0;                                                   // clear the number of parsed dmap items and cached chars,
          context->chars = 0;

          context->limit = image->resolution[0] * image->resolution[1];         // set a limit consistent with the allocated depth map size,

          XML_SetCharacterDataHandler (parser, gcode_xml_char);                 // and install a handler to load 'character data' into dmap.
        }
      }
    }
  }

  if (new_block)                                                                // If a non-null new block has been created,
    context->block = new_block;                                                 // update the context's 'current block' pointer.

  context->modus = GCODE_XML_ATTACH_UNDER;                                      // An opened element accepts new blocks UNDER itself (at its 'listhead').
}

static void
gcode_xml_end (void *parser, const char *tag)
{
  xml_context_t *context = (xml_context_t *) XML_GetUserData (parser);          // Retrieve the context via the supplied parser reference;

  if (strcmp (tag, GCODE_XML_TAG_IMAGE) == 0)                                   // 'IMAGE' end tag found...
  {
    XML_SetCharacterDataHandler (parser, NULL);                                 // Unhook the 'character data' handler (not used outside 'image' tags);

    context->index += 2;                                                        // Undo the last change of the index to get the actual number of items;

    if (context->index < context->limit)                                        // If it's less than the declared resolution, something went wrong.
    {
      REMARK ("Failed to load expected amount of image data (%i out of %i)\n", context->index, context->limit);
    }
  }
  else if (strcmp (tag, GCODE_XML_TAG_PROJECT) == 0)                            // 'PROJECT' end tag found...
  {
    if ((context->state & GCODE_XML_FLAGS_NEEDED) == GCODE_XML_FLAGS_NEEDED)    // ...so if all needed tags are present,
      context->error = 0;                                                       // mark context as 'no errors'.
  }

  if (strcmp (tag, GCODE_XML_TAG_EXTRUSION) == 0)                               // 'EXTRUSION' end tag found...
  {                                                                             // ...and these are a bit special: jumping one level back will not do.
    if (context->block)                                                         // Hopefully there is a non-NULL current block and if so,
      while (context->block->type != GCODE_TYPE_EXTRUSION)                      // it can be either the extrusion or one of its children;
      {
        if (context->block->parent)
          context->block = context->block->parent;                              // So keep backing up until the current block IS the extrusion,
        else                                                                    // or there's nowhere left to back up (that means we're screwed:
          break;                                                                // since there SHOULD have been one, we are obviously derailed).
      }

    if (context->block)                                                         // Assuming the current block is now the extrusion,
      if (context->block->parent)                                               // and everything still looks good and non-NULL-like,
        context->block = context->block->parent;                                // we need to back up one more level to continue from there.

    context->modus = GCODE_XML_ATTACH_UNDER;                                    // The next block needs to go UNDER the extrusion's parent.
  }
  else                                                                          // For any other element, apply the default rule:
  {
    if (context->modus == GCODE_XML_ATTACH_AFTER)                               // If this tag is following another closing tag,
      if (context->block)                                                       // and the current block is not NULL,
        if (context->block->parent)                                             // and there are levels left above the current one,
          context->block = context->block->parent;                              // back up one level to the current block's parent.

    context->modus = GCODE_XML_ATTACH_AFTER;                                    // A closed element accepts new blocks AFTER itself (at its 'next').
  }
}

void
gcode_init (gcode_t *gcode)
{
  strcpy (gcode->name, "");
  strcpy (gcode->notes, "");

  gcode->units = GCODE_UNITS_MILLIMETER;
  gcode->material_type = GCODE_MATERIAL_STEEL;

  gcode->material_size[0] = 1.0;                                                // For the record, 0.0 is NOT a safe value
  gcode->material_size[1] = 1.0;
  gcode->material_size[2] = 1.0;

  gcode->material_origin[0] = 0.0;
  gcode->material_origin[1] = 0.0;
  gcode->material_origin[2] = 0.0;

  gcode->ztraverse = 0.0;                                                       // Depth at which to traverse along XY plane

  gcode->gui = NULL;
  gcode->listhead = NULL;

  gcode->progress_callback = NULL;
  gcode->message_callback = NULL;

  gcode->zero_offset.side = 0.0;                                                // This only exists so new blocks have something to link to
  gcode->zero_offset.tool = 0.0;
  gcode->zero_offset.eval = 0.0;
  gcode->zero_offset.rotation = 0.0;
  gcode->zero_offset.origin[0] = 0.0;
  gcode->zero_offset.origin[1] = 0.0;
  gcode->zero_offset.z[0] = 0.0;
  gcode->zero_offset.z[1] = 0.0;

  gcode->voxel_resolution = 0;

  gcode->voxel_number[0] = 0;
  gcode->voxel_number[1] = 0;
  gcode->voxel_number[2] = 0;

  gcode->voxel_map = NULL;

  gcode->tool_xpos = FLT_MAX;
  gcode->tool_ypos = FLT_MAX;
  gcode->tool_zpos = FLT_MAX;

  gcode->format = GCODE_FORMAT_TBD;                                             // Initial file format = "to be determined" (at first save or load)
  gcode->driver = GCODE_DRIVER_LINUXCNC;

  strcpy (gcode->machine_name, "");

  gcode->machine_options = 0;
  gcode->decimals = 5;
  gcode->project_number = 0;
}

void
gcode_prep (gcode_t *gcode)
{
  uint32_t size;
  gcode_vec3d_t portion;
  gfloat_t inv_den;

  inv_den = 1.0 / (gcode->material_size[0] + gcode->material_size[1] + gcode->material_size[2]);

  portion[0] = gcode->material_size[0] * inv_den;
  portion[1] = gcode->material_size[1] * inv_den;
  portion[2] = gcode->material_size[2] * inv_den;

  /* Setup voxels */
  gcode->voxel_number[0] = (uint16_t)(gcode->voxel_resolution * portion[0]);
  gcode->voxel_number[1] = (uint16_t)(gcode->voxel_resolution * portion[1]);
  gcode->voxel_number[2] = (uint16_t)(gcode->voxel_resolution * portion[2]);

  if (gcode->voxel_number[0] == 0)
    gcode->voxel_number[0] = 1;

  if (gcode->voxel_number[1] == 0)
    gcode->voxel_number[1] = 1;

  if (gcode->voxel_number[2] == 0)
    gcode->voxel_number[2] = 1;

  size = gcode->voxel_number[0] * gcode->voxel_number[1] * gcode->voxel_number[2];

  gcode->voxel_map = (uint8_t *)realloc (gcode->voxel_map, size);
  memset (gcode->voxel_map, 1, size);
}

void
gcode_free (gcode_t *gcode)
{
  gcode_list_free (&gcode->listhead);
  free (gcode->voxel_map);
  gcode->voxel_map = NULL;
}

static void
gcode_crlf (char **string)
{
  char *crlf_string;
  int i, n, len;

  crlf_string = malloc (2 * strlen (*string) + 1);

  len = strlen (*string) + 1;
  n = 0;

  for (i = 0; i < len; i++)
  {
    crlf_string[n] = (*string)[i];
    n++;

    if (crlf_string[n - 1] == '\n')
    {
      crlf_string[n - 1] = '\r';
      crlf_string[n] = '\n';
      n++;
    }
  }

  *string = realloc (*string, strlen (crlf_string) + 1);

  memcpy (*string, crlf_string, strlen (crlf_string) + 1);

  free (crlf_string);
}

int
gcode_save (gcode_t *gcode, const char *filename)
{
  FILE *fh;
  char *fileext;
  uint32_t header, fsize, version, size, marker;
  gcode_block_t *index_block;
  uint8_t data, type;

  fh = fopen (filename, "wb");

  if (!fh)
  {
    return (1);
  }

  if (gcode->format == GCODE_FORMAT_TBD)                                        // If the file format is not determined yet, choose one based on the file extension
  {
    gcode->format = GCODE_FORMAT_BIN;                                           // Unless subsequently changed, set format to binary by default

    fileext = strrchr (filename, '.');                                          // Get the file extension, and make sure it's NOT NULL

    if (fileext)                                                                // Apparently strcmp cannot handle NULL - yes, well, NOW I know thanks a lot... 
      if (strcmp (fileext, GCODE_XML_FILETYPE) == 0)                            // Anyway - if the XML extension is detected, change the format to XML
        gcode->format = GCODE_FORMAT_XML;
  }

  if (gcode->format == GCODE_FORMAT_XML)                                        // Save to new xml format
  {
    int indent = 0;

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_PROLOG_LINE (fh, GCODE_XML_PROLOG);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_REMARK_LINE (fh, GCODE_XML_FIRST_COMMENT);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_REMARK_JOIN (fh, GCODE_XML_SECOND_COMMENT, VERSION);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_REMARK_LINE (fh, GCODE_XML_THIRD_COMMENT);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_PROJECT);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_PROJECT_VERSION, GCODE_VERSION);
    GCODE_WRITE_XML_OP_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    indent++;

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_GCODE);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_GCODE_NAME, gcode->name);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_GCODE_UNITS, gcode->units);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_GCODE_MATERIAL_TYPE, gcode->material_type);
    GCODE_WRITE_XML_ATTR_3D_FLT (fh, GCODE_XML_ATTR_GCODE_MATERIAL_SIZE, gcode->material_size);
    GCODE_WRITE_XML_ATTR_3D_FLT (fh, GCODE_XML_ATTR_GCODE_MATERIAL_ORIGIN, gcode->material_origin);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_GCODE_Z_TRAVERSE, gcode->ztraverse);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_GCODE_NOTES, gcode->notes);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_GCODE_MACHINE_NAME, gcode->machine_name);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_GCODE_MACHINE_OPTIONS, gcode->machine_options);
    GCODE_WRITE_XML_OP_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    /**
     * In the XML branch the parent does NOT save even the common attributes
     * of its child blocks (as the binary branch does): saving some attributes
     * here and some (custom) attributes in the child would get rather messy
     */

    index_block = gcode->listhead;

    while (index_block)
    {
      index_block->save (index_block, fh);

      index_block = index_block->next;
    }

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_GCODE);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    indent--;

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_PROJECT);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    header = GCODE_BIN_FILE_HEADER;
    fwrite (&header, sizeof (uint32_t), 1, fh);

    fsize = 0;
    fwrite (&fsize, sizeof (uint32_t), 1, fh);

    /**
     * VERSION
     */
    version = GCODE_VERSION;
    fwrite (&version, sizeof (uint32_t), 1, fh);

    /**
     * GCODE_DATA
     */
    type = GCODE_BIN_DATA;
    fwrite (&type, sizeof (uint8_t), 1, fh);
    marker = ftell (fh);
    size = 0;
    fwrite (&size, sizeof (uint32_t), 1, fh);

    GCODE_WRITE_BINARY_STR_DATA (fh, GCODE_BIN_DATA_NAME, gcode->name);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_UNITS, sizeof (uint8_t), &gcode->units);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_MATERIAL_TYPE, sizeof (uint8_t), &gcode->material_type);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_MATERIAL_SIZE, 3 * sizeof (gfloat_t), gcode->material_size);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_MATERIAL_ORIGIN, 3 * sizeof (gfloat_t), gcode->material_origin);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_ZTRAVERSE, sizeof (gfloat_t), &gcode->ztraverse);

    {
      uint16_t nlen;

      data = GCODE_BIN_DATA_NOTES;
      size = sizeof (uint16_t) + strlen (gcode->notes) + 1;
      fwrite (&data, sizeof (uint8_t), 1, fh);
      fwrite (&size, sizeof (uint32_t), 1, fh);
      nlen = strlen (gcode->notes) + 1;
      fwrite (&nlen, sizeof (uint16_t), 1, fh);
      fwrite (gcode->notes, nlen, 1, fh);
    }

    size = ftell (fh) - marker - sizeof (uint32_t);
    fseek (fh, marker, SEEK_SET);
    fwrite (&size, sizeof (uint32_t), 1, fh);
    fseek (fh, marker + size + sizeof (uint32_t), SEEK_SET);

    /**
     * GCODE_MACHINE_DATA
     */
    type = GCODE_BIN_DATA_MACHINE;
    fwrite (&type, sizeof (uint8_t), 1, fh);
    marker = ftell (fh);
    size = 0;
    fwrite (&size, sizeof (uint32_t), 1, fh);

    GCODE_WRITE_BINARY_STR_DATA (fh, GCODE_BIN_DATA_MACHINE_NAME, gcode->machine_name);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_MACHINE_OPTIONS, sizeof (uint8_t), &gcode->machine_options);

    size = ftell (fh) - marker - sizeof (uint32_t);
    fseek (fh, marker, SEEK_SET);
    fwrite (&size, sizeof (uint32_t), 1, fh);
    fseek (fh, marker + size + sizeof (uint32_t), SEEK_SET);

    index_block = gcode->listhead;

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

    /* Write the actual length */
    fsize = ftell (fh);
    fseek (fh, sizeof (uint32_t), SEEK_SET);
    fwrite (&fsize, sizeof (uint32_t), 1, fh);
  }

  fclose (fh);

  return (0);
}

int
gcode_load (gcode_t *gcode, const char *filename)
{
  FILE *fh;
  int result;
  XML_Parser parser;
  uint32_t header, fsize, version, size;
  uint8_t data, type;
  xml_context_t *context;
  gcode_block_t *new_block;

  result = 1;

  fh = fopen (filename, "rb");

  if (!fh)
  {
    REMARK ("Failed to open file '%s'\n", basename ((char *)filename));
    return (result);
  }

  /**
   * Set the numeric locale to C (Minimal) so that all numeric output
   * uses the period as the decimal separator since most G-Gcode drivers
   * are only compatible with a period decimal separator.  This allows
   * the user interface in GCAM to display a comma decimal separator
   * for locales that call for that format.
   */

  setlocale (LC_NUMERIC, "C");

  switch (gcode->format)
  {
    case GCODE_FORMAT_BIN:

      new_block = NULL;

      fread (&header, sizeof (uint32_t), 1, fh);

      if (header == GCODE_BIN_FILE_HEADER)
      {
        fread (&fsize, sizeof (uint32_t), 1, fh);

        fread (&version, sizeof (uint32_t), 1, fh);

        while (ftell (fh) < fsize)
        {
          /* Read Data */
          fread (&type, sizeof (uint8_t), 1, fh);

          switch (type)
          {
            case GCODE_BIN_DATA:
            {
              uint32_t start;

              fread (&size, sizeof (uint32_t), 1, fh);

              start = ftell (fh);

              while (ftell (fh) - start < size)
              {
                uint32_t dsize;

                fread (&data, sizeof (uint8_t), 1, fh);
                fread (&dsize, sizeof (uint32_t), 1, fh);

                switch (data)
                {
                  case GCODE_BIN_DATA_NAME:
                    fread (&gcode->name, sizeof (char), dsize, fh);
                    break;

                  case GCODE_BIN_DATA_UNITS:
                    fread (&gcode->units, sizeof (uint8_t), 1, fh);
                    break;

                  case GCODE_BIN_DATA_MATERIAL_TYPE:
                    fread (&gcode->material_type, sizeof (uint8_t), 1, fh);
                    break;

                  case GCODE_BIN_DATA_MATERIAL_SIZE:
                    fread (&gcode->material_size, dsize, 1, fh);
                    break;

                  case GCODE_BIN_DATA_MATERIAL_ORIGIN:
                    fread (&gcode->material_origin, dsize, 1, fh);
                    break;

                  case GCODE_BIN_DATA_ZTRAVERSE:
                    fread (&gcode->ztraverse, dsize, 1, fh);
                    break;

                  case GCODE_BIN_DATA_NOTES:
                  {
                    uint16_t nlen;

                    fread (&nlen, sizeof (uint16_t), 1, fh);
                    fread (gcode->notes, nlen, 1, fh);
                    break;
                  }

                  default:
                    fseek (fh, dsize, SEEK_CUR);
                    break;
                }
              }

              break;
            }

            case GCODE_BIN_DATA_MACHINE:
            {
              uint32_t start;

              fread (&size, sizeof (uint32_t), 1, fh);

              start = ftell (fh);

              while (ftell (fh) - start < size)
              {
                uint32_t dsize;

                fread (&data, sizeof (uint8_t), 1, fh);
                fread (&dsize, sizeof (uint32_t), 1, fh);

                switch (data)
                {
                  case GCODE_BIN_DATA_MACHINE_NAME:
                    fread (&gcode->machine_name, sizeof (char), dsize, fh);
                    break;

                  case GCODE_BIN_DATA_MACHINE_OPTIONS:
                    fread (&gcode->machine_options, dsize, 1, fh);
                    break;

                  default:
                    fseek (fh, dsize, SEEK_CUR);
                    break;
                }
              }
              break;
            }

            case GCODE_TYPE_BEGIN:
              gcode_begin_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_END:
              gcode_end_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_TOOL:
              gcode_tool_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_CODE:
              gcode_code_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_EXTRUSION:
              /* should never be called as a top level block */
              break;

            case GCODE_TYPE_SKETCH:
              gcode_sketch_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_LINE:
              /* should never be called as a top level block */
              break;

            case GCODE_TYPE_ARC:
              /* should never be called as a top level block */
              break;

            case GCODE_TYPE_BOLT_HOLES:
              gcode_bolt_holes_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_TEMPLATE:
              gcode_template_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_DRILL_HOLES:
              gcode_drill_holes_init (&new_block, gcode, NULL);
              break;

            case GCODE_TYPE_POINT:
              /* should never be called as a top level block */
              break;

            case GCODE_TYPE_IMAGE:
              gcode_image_init (&new_block, gcode, NULL);
              break;

            default:
              fread (&size, sizeof (uint32_t), 1, fh);
              fseek (fh, size, SEEK_CUR);
              break;
          }

          /* Append block to list */
          if (type != GCODE_BIN_DATA && type != GCODE_BIN_DATA_MACHINE)
          {
            gcode_append_as_listtail (NULL, new_block);                         // Keep appending each new block to the end of 'gcode's' list
                                                                                // as a consequence of calling 'append' with a NULL param.
            new_block->load (new_block, fh);
          }
        }

        result = 0;
      }

      break;

    case GCODE_FORMAT_XML:

      parser = XML_ParserCreate (NULL);                                         // The file to be loaded SHOULD contain an encoding declaration

      if (parser)                                                               // If the parser was created successfully, carry on
      {
        context = gcode_xml_create_context (gcode);                             // Create and initialize a parser context structure

        if (context)                                                            // If the context was created succesfully, get to work
        {
          XML_SetElementHandler (parser, gcode_xml_start, gcode_xml_end);       // Set the element start and element end handlers
          XML_SetUserData (parser, context);                                    // Register the context as the thing to pass to the handlers
          XML_UseParserAsHandlerArg (parser);                                   // Request a parser reference to be passed to the handlers

          for (;;)                                                              // Keep looping until the file ends or something goes wrong
          {
            int status;
            size_t units;

            void *buffer = XML_GetBuffer (parser, GCODE_XML_BUFFER_SIZE);       // Acquire a buffer to read a section of the file into.

            if (!buffer)                                                        // If we get a NULL pointer for a buffer, 
            {                                                                   // this is clearly not going to work;
              REMARK ("Failed to allocate memory for XML buffer\n");            // throw an error and leave.
              break;
            }

            units = fread (buffer, 1, GCODE_XML_BUFFER_SIZE, fh);

            if (units < 0)                                                      // Docs are not clear on whether this can happen,
            {                                                                   // but if it does, it's definitely... not good;
              REMARK ("Failed to read file '%s'\n", basename ((char *)filename));
              break;
            }

            status = XML_Parse (parser, buffer, units, units == 0);

            if (status == XML_STATUS_ERROR)
            {
              REMARK ("XML parse error in file '%s' at line %d: %s\n", basename ((char *)filename), (int)XML_GetCurrentLineNumber (parser), XML_ErrorString (XML_GetErrorCode (parser)));
              break;
            }

            if (units == 0)                                                     // If number of bytes read is zero and we're still here,
            {                                                                   // it seems for better or worse we did parse the file,
              result = 0;                                                       // so let's call this a success and leave.
              break;
            }
          }

          if (context->error)                                                   // If the handlers returned 'error',
          {
            if (context->gcode->listhead)                                       // and the current listhead is not NULL,
            {
              gcode_list_free (&context->gcode->listhead);                      // it bloody well should be: unroll it.
            }

            if (!(context->state & GCODE_XML_FLAG_PROJECT))                     // Try to guess what may have gone wrong;
            {
              REMARK ("No '%s' element found in file '%s'\n", GCODE_XML_TAG_PROJECT, basename ((char *)filename));
            }
            else if (!(context->state & GCODE_XML_FLAG_GCODE))
            {
              REMARK ("No acceptable '%s' element found in file '%s'\n", GCODE_XML_TAG_GCODE, basename ((char *)filename));
            }
            else if (!(context->state & GCODE_XML_FLAG_BEGIN))
            {
              REMARK ("No acceptable '%s' element found in file '%s'\n", GCODE_XML_TAG_BEGIN, basename ((char *)filename));
            }
            else if (!(context->state & GCODE_XML_FLAG_END))
            {
              REMARK ("No acceptable '%s' element found in file '%s'\n", GCODE_XML_TAG_END, basename ((char *)filename));
            }

            result = 1;                                                         // Also, we should return 'error' too.
          }

          free (context);
        }
        else                                                                    // If the context was not created successfully,
        {
          REMARK ("Failed to allocate memory for XML context\n");               // throw an error and proceed to clean stuff up.
        }

        XML_ParserFree (parser);
      }
      else                                                                      // If the parser was not created successfully,
      {
        REMARK ("Failed to allocate memory for XML parser\n");                  // throw an error and proceed to clean stuff up.
      }

      break;
  }

  /* Making the entire resulting G-code now does not seem very just-in-time at all */

  fclose (fh);

  /* Restore Locale to previous value */
  setlocale (LC_NUMERIC, "");

  return (result);
}

int
gcode_export (gcode_t *gcode, const char *filename)
{
  gcode_block_t *index_block;
  FILE *fh;
  int size;

  fh = fopen (filename, "w");

  if (!fh)
    return (1);

  /**
   * Check whether this is a windows machine or not by checking whether
   * writing \n results in \r\n in the file (2 bytes).
   */
  fprintf (fh, "\n");
  fseek (fh, 0, SEEK_END);
  size = ftell (fh);

  fseek (fh, 0, SEEK_SET);

  /**
   * Set appropriate number of decimals given driver
   */
  switch (gcode->driver)
  {
    case GCODE_DRIVER_HAAS:
      gcode->decimals = 4;
      break;

    default:
      gcode->decimals = 5;
      break;
  }

  /* Make all */
  gcode_list_make (gcode);

  index_block = gcode->listhead;

  while (index_block)
  {
    if (size == 1)
      gcode_crlf (&index_block->code);

    fwrite (index_block->code, 1, strlen (index_block->code), fh);

    index_block = index_block->next;
  }

  fclose (fh);

  if (gcode->progress_callback)
    gcode->progress_callback (gcode->gui, 0.0);

  return (0);
}

void
gcode_render_final (gcode_t *gcode, gfloat_t *time_elapsed)
{
  char *source, line[256], *sp, *tsp, *gv;
  gcode_block_t *index_block;
  gcode_sim_t sim;
  uint32_t size, line_num, line_ind, mode = 0;
  gfloat_t G83_depth = 0.0, G83_retract = 0.0;

  /* Make all */
  gcode_list_make (gcode);

  gcode_sim_init (&sim, gcode);

  GCODE_MATH_VEC3D_SET (sim.vn_inv, 1.0 / (gfloat_t)gcode->voxel_number[0], 1.0 / (gfloat_t)gcode->voxel_number[1], 1.0 / (gfloat_t)gcode->voxel_number[2]);

  /* Turn all the voxels back on */
  size = gcode->voxel_number[0] * gcode->voxel_number[1] * gcode->voxel_number[2];
  memset (gcode->voxel_map, 1, size);

  source = (char *)malloc (1);
  source[0] = 0;

  index_block = gcode->listhead;

  while (index_block)
  {
    source = (char *)realloc (source, strlen (source) + strlen (index_block->code) + 1);
    strcat (source, index_block->code);

    index_block = index_block->next;
  }

  /* Count the number of lines */
  line_num = 0;
  sp = source;

  while ((tsp = strchr (sp, '\n')))
  {
    sp = tsp + 1;
    line_num++;
  }

  /* Isolate each line */
  line_ind = 0;
  sp = source;

  while ((tsp = strchr (sp, '\n')))
  {
    uint8_t sind;

    if (gcode->progress_callback)
      gcode->progress_callback (gcode->gui, (gfloat_t)line_ind++ / (gfloat_t)line_num);

    memset (line, 0, 256);
    memcpy (line, sp, tsp - sp);
    sp = tsp + 1;
    sind = 0;

    /**
     * Parse the line
     */

    /* Scan for comments of the form (Tool diameter: VALUE) */
    gv = strstr (line, "Tool Diameter:");

    if (gv)
    {
      char string[256];
      uint8_t len;

      gv = strpbrk (line, ".0123456789");
      len = strspn (gv, ".0123456789");
      memset (string, 0, 256);
      memcpy (string, gv, len);
      sim.tool_diameter = atof (string);
    }

    /* Scan for comments of the form (Origin offset: X=VALUE Y=VALUE Z=VALUE) */
    gv = strstr (line, "Origin Offset:");

    if (gv)
    {
      char string[256];
      uint8_t len;

      gv = strpbrk (gv, ".0123456789");
      len = strspn (gv, ".0123456789");
      memset (string, 0, 256);
      memcpy (string, gv, len);
      sim.origin[0] = atof (string);
      sim.pos[0] += sim.origin[0];

      gv += len;
      gv = strpbrk (gv, ".0123456789");
      len = strspn (gv, ".0123456789");
      memset (string, 0, 256);
      memcpy (string, gv, len);
      sim.origin[1] = atof (string);
      sim.pos[1] += sim.origin[1];

      gv += len;
      gv = strpbrk (gv, ".0123456789");
      len = strspn (gv, ".0123456789");
      memset (string, 0, 256);
      memcpy (string, gv, len);
      sim.origin[2] = atof (string);
      sim.pos[2] += sim.origin[2];
    }

    /* Remove the spaces */
    gcode_util_remove_spaces (line);

    /* Strip comments */
    gcode_util_remove_comment (line);

    switch (line[sind])
    {
      case 'G':
      {
        uint8_t len;
        char string[256];

        sind++;

        /* The Number */
        len = strspn (&line[sind], "0123456789");
        memset (string, 0, 256);
        memcpy (string, &line[sind], len);
        sind += len;

        switch (atoi (string))
        {
          case 0:
            gcode_sim_G00 (gcode, &sim, &line[sind]);
            break;

          case 1:
            gcode_sim_G01 (gcode, &sim, &line[sind]);
            break;

          case 2:
            gcode_sim_G02 (gcode, &sim, &line[sind]);
            break;

          case 3:
            gcode_sim_G03 (gcode, &sim, &line[sind]);
            break;

          case 4:
            /* Dwell */
            break;

          case 20:
            break;

          case 21:
            break;

          case 81:
          case 83:
            gcode_sim_G83 (gcode, &sim, &line[sind], &G83_depth, &G83_retract, 1);
            mode = 83;
            break;

          case 90:
            sim.absolute = 1;
            break;

          case 91:
            sim.absolute = 0;
            break;

          default:
            break;
        }
        break;
      }

      case 'F':
      {
        char string[256];
        uint8_t len;

        sind++;

        len = strspn (&line[sind], ".0123456789");
        memset (string, 0, 256);
        memcpy (string, &line[sind], len);
        sim.feed = atof (string);
        break;
      }

      case 'X':
      case 'Y':
      {
        if (mode == 83)
        {
          gcode_sim_G83 (gcode, &sim, &line[sind], &G83_depth, &G83_retract, 0);
        }
        break;
      }

      default:
        break;
    }
  }

  free (source);

  /* Calculate elapsed time */
  sim.time_elapsed = 60 * sim.time_elapsed / sim.feed;

  *time_elapsed = sim.time_elapsed;
  gcode_sim_free (&sim);
}

void
gcode_dump_tree (gcode_t *gcode, gcode_block_t *block)
{
  gcode_block_t *index_block;

  if (block)
  {
    index_block = block;
  }
  else
  {
    index_block = gcode->listhead;

    printf ("GCODE address: 0x%.8X, default-offset address: 0x%.8X\n", (intptr_t)gcode, (intptr_t)(&gcode->zero_offset));
  }

  while (index_block)
  {
    printf ("Block address: 0x%.8X, "
            "name: 0x%.8X, "
            "gcode: 0x%.8X, "
            "parent: 0x%.8X, "
            "prev: 0x%.8X, "
            "next: 0x%.8X, "
            "extruder: 0x%.8X, "
            "listhead: 0x%.8X, "
            "pdata: 0x%.8X, "
            "offref: 0x%.8X, "
            "offset: 0x%.8X, "
            "type: '%s'\n",
            (intptr_t)index_block,
            index_block->name,
            (intptr_t)index_block->gcode,
            (intptr_t)index_block->parent,
            (intptr_t)index_block->prev,
            (intptr_t)index_block->next,
            (intptr_t)index_block->extruder,
            (intptr_t)index_block->listhead,
            (intptr_t)index_block->pdata,
            (intptr_t)index_block->offref,
            (intptr_t)index_block->offset,
            GCODE_TYPE_STRING[index_block->type]);

    if (index_block->extruder)
      gcode_dump_tree (gcode, index_block->extruder);

    if (index_block->listhead)
      gcode_dump_tree (gcode, index_block->listhead);

    index_block = index_block->next;
  }

  fflush (stdout);
}
