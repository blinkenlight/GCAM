/**
 *  gcode_bolt_holes.c
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

#include "gcode_bolt_holes.h"
#include "gcode_extrusion.h"
#include "gcode_tool.h"
#include "gcode_arc.h"
#include "gcode_pocket.h"
#include "gcode.h"

void
gcode_bolt_holes_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_bolt_holes_t *bolt_holes;
  gcode_block_t *extrusion_block;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_BOLT_HOLES, 0);

  (*block)->free = gcode_bolt_holes_free;
  (*block)->save = gcode_bolt_holes_save;
  (*block)->load = gcode_bolt_holes_load;
  (*block)->make = gcode_bolt_holes_make;
  (*block)->draw = gcode_bolt_holes_draw;
  (*block)->aabb = gcode_bolt_holes_aabb;
  (*block)->move = gcode_bolt_holes_move;
  (*block)->spin = gcode_bolt_holes_spin;
  (*block)->scale = gcode_bolt_holes_scale;
  (*block)->parse = gcode_bolt_holes_parse;
  (*block)->clone = gcode_bolt_holes_clone;

  (*block)->pdata = malloc (sizeof (gcode_bolt_holes_t));

  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Bolt Holes");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  bolt_holes = (gcode_bolt_holes_t *)(*block)->pdata;

  bolt_holes->position[0] = 0.0;
  bolt_holes->position[1] = 0.0;
  bolt_holes->number[0] = 4;
  bolt_holes->number[1] = 4;
  bolt_holes->hole_diameter = GCODE_UNITS ((*block)->gcode, 0.25);
  bolt_holes->type = GCODE_BOLT_HOLES_TYPE_RADIAL;
  bolt_holes->offset_distance = GCODE_UNITS ((*block)->gcode, 0.5);
  bolt_holes->offset_angle = 0.0;
  bolt_holes->pocket = 0;

  bolt_holes->offset.side = -1.0;
  bolt_holes->offset.tool = 0.0;
  bolt_holes->offset.eval = 0.0;
  bolt_holes->offset.rotation = 0.0;
  bolt_holes->offset.origin[0] = 0.0;
  bolt_holes->offset.origin[1] = 0.0;
  bolt_holes->offset.z[0] = 0.0;
  bolt_holes->offset.z[1] = 0.0;

  (*block)->offref = &bolt_holes->offset;

  /* Create default extrusion */

  gcode_extrusion_init (&extrusion_block, gcode, *block);

  gcode_attach_as_extruder (*block, extrusion_block);

  gcode_bolt_holes_rebuild (*block);
}

void
gcode_bolt_holes_free (gcode_block_t **block)
{
  gcode_block_t *index_block, *tmp;

  /* Free the extrusion list */
  (*block)->extruder->free (&(*block)->extruder);

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
gcode_bolt_holes_make (gcode_block_t *block)
{
  gcode_bolt_holes_t *bolt_holes;
  gcode_extrusion_t *extrusion;
  gcode_tool_t *tool;
  gcode_block_t *offset_block;
  gcode_block_t *index_block;
  gcode_vec2d_t p0, p1, e0, e1, cp;
  gfloat_t z, z0, z1, safe_z, touch_z;
  gfloat_t tool_radius;
  char string[256];
  int number, initial;

  GCODE_CLEAR (block);                                                          // Clean up the g-code string of this block to an empty string;

  if (!block->listhead)                                                         // If the list is empty, this will definitely be a quick pass...
    return;

  if (block->flags & GCODE_FLAGS_SUPPRESS)                                      // Same thing if the bolt_holes is currently not supposed to be made;
    return;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;                              // Get references to the data structs of the bolt_holes and its extrusion;
  extrusion = (gcode_extrusion_t *)block->extruder->pdata;

  tool = gcode_tool_find (block);                                               // Find the tool that applies to this block;

  if (!tool)                                                                    // If there is none, this block will not get made - bail out;
    return;

  tool_radius = tool->diameter * 0.5;                                           // If a tool was found, obtain its radius;

  GCODE_NEWLINE (block);                                                        // Print some generic info and newlines into the g-code string;

  sprintf (string, "BOLT HOLES: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  block->extruder->ends (block->extruder, p0, p1, GCODE_GET);                   // Find the start and end depth of the extrusion curve;

  if (p0[1] > p1[1])                                                            // If the first depth is above the second, store the z-values as they are;
  {
    z0 = p0[1];
    z1 = p1[1];
  }
  else                                                                          // If the first depth is below the second, swap the z-values before storing;
  {
    z0 = p1[1];
    z1 = p0[1];
  }

  bolt_holes->offset.origin[0] = block->offset->origin[0];                      // Inherit the offset of the parent by copying it into this block's offset;
  bolt_holes->offset.origin[1] = block->offset->origin[1];
  bolt_holes->offset.rotation = block->offset->rotation;

  bolt_holes->offset.side = -1.0;                                               // The offset side is always "inside" (it matters for tapered extrusions);
  bolt_holes->offset.tool = tool_radius;                                        // Making very much depends on the tool size - set it up;

  safe_z = block->gcode->ztraverse;                                             // This is just a short-hand for the traverse z...

  touch_z = block->gcode->material_origin[2];                                   // Track the depth the material begins at (gets lower after every pass);

  if (fabs (bolt_holes->hole_diameter - tool->diameter) < GCODE_PRECISION)      // Start of drill cycle (G81 - if the tool can create the holes by drilling);
  {
    if (block->gcode->drilling_motion == GCODE_DRILLING_CANNED)
    {
      GCODE_DRILL (block, z1, tool->feed * tool->plunge_ratio, safe_z);
    }
  }

  number = 1;                                                                   // Astonishingly, human beings tend to start counting from "1", not "0"...

  index_block = block->listhead;                                                // Start crawling along the list of holes (arcs);

  while (index_block)                                                           // Keep looping as long as the list lasts, one hole per loop;
  {
    if (fabs (bolt_holes->hole_diameter - tool->diameter) < GCODE_PRECISION)    // See if the hole can be created by drilling with this tool;
    {
      if (index_block->type == GCODE_TYPE_ARC)                                  // This should always be true - there can only be arcs here...
      {
        gcode_arc_center (index_block, cp, GCODE_GET_WITH_OFFSET);              // Get the (offset) position of the center of this arc / hole;

        sprintf (string, "hole #%d", number);

        if (block->gcode->drilling_motion == GCODE_DRILLING_CANNED)             // If "canned" cycles are allowed,
        {
          GCODE_XY_PAIR (block, cp[0], cp[1], string);                          // punch out a "canned" hole at the position of the center;
        }
        else                                                                    // If they are not, emulate them with simple motions;
        {
          GCODE_MOVE_TO (block, cp[0], cp[1], z1, safe_z, touch_z, tool, string);
        }
      }
    }
    else                                                                        // If the tool cannot drill this hole, mill it out instead;
    {
      GCODE_NEWLINE (block);

      sprintf (string, "Hole #%d", number);
      GCODE_COMMENT (block, string);                                            // This may take a while: throw in some comments at the start;

      GCODE_NEWLINE (block);

      initial = 1;                                                              // For the first pass (whether it is a 'zero pass' or not) this stays true;

      if (z0 - z1 > extrusion->resolution)                                      // Start one step deeper than z0 if the total depth is larger than one step;
        z = z0 - extrusion->resolution;
      else                                                                      // If even a single step would be too deep, start the pass at z1 instead;
        z = z1;

      GCODE_RETRACT (block, safe_z);                                            // Retract - should already be retracted, but here for safety reasons.

      while (z >= z1)                                                           // Loop on the current hole, creating one pass depth per loop;
      {
        GCODE_NEWLINE (block);

        gsprintf (string, block->gcode->decimals, "Pass at depth: %z", z);
        GCODE_COMMENT (block, string);

        GCODE_NEWLINE (block);

        gcode_extrusion_evaluate_offset (block->extruder, z, &bolt_holes->offset.eval); // Get the extrusion profile offset calculated for the current z-depth;

        gcode_util_get_sublist_snapshot (&offset_block, index_block, index_block);      // We need a working snapshot of... uhhh... a single block?!? Oh well...

        gcode_util_convert_to_no_offset (offset_block);                         // Recalculate that snapshot with offsets included & link it to a zero offset;

        if (bolt_holes->pocket)                                                 // Perform pocketing if requested
        {
          gcode_pocket_t pocket;

          gcode_pocket_init (&pocket, block, tool);                             // Create a pocket for 'offset_block';
          gcode_pocket_prep (&pocket, offset_block, NULL);                      // Create a raster of paths based on the contour;
          gcode_pocket_make (&pocket, z, touch_z);                              // Create the g-code from the pocket's path list;
          gcode_pocket_free (&pocket);                                          // Dispose of the no longer needed pocket;
        }

        /**
         * Pocketing is complete, get in position for the contour pass
         */

        offset_block->ends (offset_block, e0, e1, GCODE_GET_WITH_OFFSET);       // Get the endpoints of the hole / arc (both should be the same anyway...);

        GCODE_NEWLINE (block);

        GCODE_COMMENT (block, "Hole Contour Milling Phase");

        GCODE_NEWLINE (block);

        GCODE_MOVE_TO (block, e0[0], e0[1], z, safe_z, touch_z, tool, "start of contour");      // Once found, move to that position then plunge;

        offset_block->offset->z[0] = z;                                         // Thankfully, this is the trivial part - everything happens at 'z';
        offset_block->offset->z[1] = z;

        offset_block->make (offset_block);                                      // There is only ever 1 arc...
        GCODE_APPEND (block, offset_block->code);

        free (offset_block->offset);                                            // The specially created zero-offset is no longer needed;
        gcode_list_free (&offset_block);                                        // This depth has been built - get rid of the snapshot;

        initial = 0;                                                            // Further passes are not the 'first pass' any more;
        touch_z = z;                                                            // The current z depth is now the new boundary between air and material;

        if (z - z1 > extrusion->resolution)                                     // Go one level deeper if the remaining depth is larger than one depth step;
          z = z - extrusion->resolution;
        else if (z - z1 > GCODE_PRECISION)                                      // If it's less but still a significant amount, go to the final z1 instead;
          z = z1;
        else                                                                    // If the depth of the last pass is already indistinguishable from z1, quit;
          break;
      }

      GCODE_RETRACT (block, safe_z);                                            // Pull back up when done;
    }

    number++;                                                                   // Count the completed hole;

    index_block = index_block->next;                                            // Move on to the next hole / block;
  }

  if (fabs (bolt_holes->hole_diameter - tool->diameter) < GCODE_PRECISION)      // End of canned cycle (G80)
  {
    if (block->gcode->drilling_motion == GCODE_DRILLING_CANNED)
    {
      GCODE_COMMAND (block, "G80", "end canned cycle");
      GCODE_F_VALUE (block, tool->feed, "normal feed rate");
    }

    GCODE_RETRACT (block, safe_z);                                              // Pull back up when done;
  }

  bolt_holes->offset.side = 0.0;                                                // Not of any importance strictly speaking (anywhere these actually matter they
  bolt_holes->offset.tool = 0.0;                                                // should be re-initialized appropriately anyway), but hey - let's play nice...
  bolt_holes->offset.eval = 0.0;
}

void
gcode_bolt_holes_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_bolt_holes_t *bolt_holes;
  uint32_t size, marker;
  uint8_t data;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

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
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_BOLT_HOLES);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_BOLT_HOLES_TYPE, bolt_holes->type);
    GCODE_WRITE_XML_ATTR_1D_INT (fh, GCODE_XML_ATTR_BOLT_HOLES_POCKET, bolt_holes->pocket);
    GCODE_WRITE_XML_ATTR_2D_INT (fh, GCODE_XML_ATTR_BOLT_HOLES_NUMBER, bolt_holes->number);
    GCODE_WRITE_XML_ATTR_2D_FLT (fh, GCODE_XML_ATTR_BOLT_HOLES_POSITION, bolt_holes->position);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_BOLT_HOLES_HOLE_DIAMETER, bolt_holes->hole_diameter);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_BOLT_HOLES_OFFSET_DISTANCE, bolt_holes->offset_distance);
    GCODE_WRITE_XML_ATTR_1D_FLT (fh, GCODE_XML_ATTR_BOLT_HOLES_OFFSET_ANGLE, bolt_holes->offset_angle);
    GCODE_WRITE_XML_OP_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    /**
     * In the XML branch the parent does NOT save even the common attributes
     * of its child blocks (as the binary branch does): saving some attributes
     * here and some (custom) attributes in the child would get rather messy
     */

    block->extruder->save (block->extruder, fh);

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_BOLT_HOLES);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    /* SAVE EXTRUSION DATA */
    data = GCODE_BIN_DATA_BOLT_HOLES_EXTRUSION;
    size = 0;
    fwrite (&data, sizeof (uint8_t), 1, fh);

    /* Write block type */
    marker = ftell (fh);
    size = 0;
    fwrite (&size, sizeof (uint32_t), 1, fh);

    /* Write comment */
    GCODE_WRITE_BINARY_STR_DATA (fh, GCODE_BIN_DATA_BLOCK_COMMENT, block->extruder->comment);

    block->extruder->save (block->extruder, fh);

    size = ftell (fh) - marker - sizeof (uint32_t);
    fseek (fh, marker, SEEK_SET);
    fwrite (&size, sizeof (uint32_t), 1, fh);
    fseek (fh, marker + size + sizeof (uint32_t), SEEK_SET);

    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BOLT_HOLES_POSITION, 2 * sizeof (gfloat_t), &bolt_holes->position);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BOLT_HOLES_HOLE_DIAMETER, sizeof (gfloat_t), &bolt_holes->hole_diameter);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BOLT_HOLES_OFFSET_DISTANCE, sizeof (gfloat_t), &bolt_holes->offset_distance);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BOLT_HOLES_TYPE, sizeof (uint8_t), &bolt_holes->type);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BOLT_HOLES_NUMBER, 2 * sizeof (int), bolt_holes->number);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BOLT_HOLES_OFFSET_ANGLE, sizeof (gfloat_t), &bolt_holes->offset_angle);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_BOLT_HOLES_POCKET, sizeof (uint8_t), &bolt_holes->pocket);
  }
}

void
gcode_bolt_holes_load (gcode_block_t *block, FILE *fh)
{
  gcode_bolt_holes_t *bolt_holes;
  uint32_t bsize, dsize, start;
  uint8_t data;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

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

      case GCODE_BIN_DATA_BOLT_HOLES_EXTRUSION:
        /* Rewind 4 bytes because the extrusion wants to read in its block size too. */
        fseek (fh, -4, SEEK_CUR);
        gcode_extrusion_load (block->extruder, fh);
        break;

      case GCODE_BIN_DATA_BOLT_HOLES_POSITION:
        fread (bolt_holes->position, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_BOLT_HOLES_HOLE_DIAMETER:
        fread (&bolt_holes->hole_diameter, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_BOLT_HOLES_OFFSET_DISTANCE:
        fread (&bolt_holes->offset_distance, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_BOLT_HOLES_TYPE:
        fread (&bolt_holes->type, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_BOLT_HOLES_NUMBER:
        fread (bolt_holes->number, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_BOLT_HOLES_OFFSET_ANGLE:
        fread (&bolt_holes->offset_angle, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_BOLT_HOLES_POCKET:
        fread (&bolt_holes->pocket, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }

  gcode_bolt_holes_rebuild (block);
}

void
gcode_bolt_holes_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_bolt_holes_t *bolt_holes;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  for (int i = 0; xmlattr[i]; i += 2)
  {
    int abc[3], m;
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
    else if (strcmp (name, GCODE_XML_ATTR_BOLT_HOLES_TYPE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        bolt_holes->type = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_BOLT_HOLES_POCKET) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_INT (m, value))
        bolt_holes->pocket = m;
    }
    else if (strcmp (name, GCODE_XML_ATTR_BOLT_HOLES_NUMBER) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_INT (abc, value))
        for (int j = 0; j < 2; j++)
          bolt_holes->number[j] = abc[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_BOLT_HOLES_POSITION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_FLT (xyz, value))
        for (int j = 0; j < 2; j++)
          bolt_holes->position[j] = (gfloat_t)xyz[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_BOLT_HOLES_HOLE_DIAMETER) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        bolt_holes->hole_diameter = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_BOLT_HOLES_OFFSET_DISTANCE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        bolt_holes->offset_distance = (gfloat_t)w;
    }
    else if (strcmp (name, GCODE_XML_ATTR_BOLT_HOLES_OFFSET_ANGLE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_1D_FLT (w, value))
        bolt_holes->offset_angle = (gfloat_t)w;
    }
  }

  gcode_bolt_holes_rebuild (block);
}

/**
 * Draw the contents of the bolt_holes as several contour lines (one for each
 * hole and milling pass depth as determined by the extrusion resolution);
 * NOTE: it would be nice to check for existence of all pertinent functions like
 * 'ends', 'eval' etc. on the full list, the extrusion AND its list but for now
 * we're just going to assert all blocks involved are of a type that has those;
 * NOTE: in case you're wondering, the "CTRL-click on a hole to select its row
 * in the tree view" still works because rebuilding the arcs the holes consist
 * of gets them renamed to the name of the bolt_holes, and 'snapshots' preserve
 * the name of their original - so all arcs drawn get actually tagged with the 
 * name of the bolt_holes block, and therefore can select it when clicked on.
 */

void
gcode_bolt_holes_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_bolt_holes_t *bolt_holes;
  gcode_extrusion_t *extrusion;
  gcode_block_t *offset_block;
  gcode_block_t *index_block;
  gcode_vec2d_t p0, p1;
  gfloat_t z, z0, z1;

  if (!block->listhead)                                                         // If the list is empty, this will definitely be a quick pass...
    return;

  if (block->flags & GCODE_FLAGS_SUPPRESS)                                      // Same thing if the bolt_holes is currently not supposed to be drawn;
    return;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;                              // Get references to the data structs of the bolt_holes and its extrusion;
  extrusion = (gcode_extrusion_t *)block->extruder->pdata;

  block->extruder->ends (block->extruder, p0, p1, GCODE_GET);                   // Find the start and end depth of the extrusion curve;

  if (p0[1] > p1[1])                                                            // If the first depth is above the second, store the z-values as they are;
  {
    z0 = p0[1];
    z1 = p1[1];
  }
  else                                                                          // If the first depth is below the second, swap the z-values before storing;
  {
    z0 = p1[1];
    z1 = p0[1];
  }

  bolt_holes->offset.origin[0] = block->offset->origin[0];                      // Inherit the offset of the parent by copying it into this block's offset;
  bolt_holes->offset.origin[1] = block->offset->origin[1];
  bolt_holes->offset.rotation = block->offset->rotation;

  bolt_holes->offset.side = -1.0;                                               // The offset side is always "inside" (it matters for tapered extrusions);
  bolt_holes->offset.tool = 0.0;                                                // The drawing operation is not influenced by tool size, so set it to zero;

  index_block = block->listhead;                                                // Start crawling along the list of holes (arcs);

  while (index_block)                                                           // Keep looping as long as the list lasts, one hole per loop;
  {
    z = z0;                                                                     // Start the first pass at z0 (arguably this should start one step lower);

    while (z >= z1)                                                             // Loop on the current hole, creating one pass depth per loop;
    {
      gcode_extrusion_evaluate_offset (block->extruder, z, &bolt_holes->offset.eval);   // Get the extrusion profile offset calculated for the current z-depth;

      gcode_util_get_sublist_snapshot (&offset_block, index_block, index_block);        // We need a working snapshot of... uhhh... a single block?!? Oh well...

      gcode_util_convert_to_no_offset (offset_block);                           // Recalculate that snapshot with offsets included & link it to a zero offset;

      offset_block->offset->z[0] = z;                                           // Thankfully, this is the trivial part - everything happens at 'z';
      offset_block->offset->z[1] = z;

      offset_block->draw (offset_block, selected);                              // Snapshots RETAIN their parent - if this 'bolt_holes'
                                                                                // is selected, the draw routine WILL STILL DETECT that.
      free (offset_block->offset);                                              // The specially created zero-offset is no longer needed;
      gcode_list_free (&offset_block);                                          // This depth has been drawn - get rid of the snapshot;

      if (z - z1 > extrusion->resolution)                                       // Go one level deeper if the remaining depth is larger than one depth step;
        z = z - extrusion->resolution;
      else if (z - z1 > GCODE_PRECISION)                                        // If it's less but still a significant amount, go to the final z1 instead;
        z = z1;
      else                                                                      // If the depth of the last pass is already indistinguishable from z1, quit;
        break;
    }

    index_block = index_block->next;                                            // Move on to the next hole / block;
  }

  bolt_holes->offset.side = 0.0;                                                // Not of any importance strictly speaking (anywhere these actually matter they
  bolt_holes->offset.tool = 0.0;                                                // should be re-initialized appropriately anyway), but hey - let's play nice...
  bolt_holes->offset.eval = 0.0;
#endif
}

/**
 * Construct the axis-aligned bounding box of all the holes in the bolt holes;
 * NOTE: this can and does return an "imposible" or "inside-out" bounding box
 * which has its minimum larger than its maximum as a sign of failure to pick
 * up any valid holes, if either the list is empty or none of the members are 
 * arcs (there should ONLY be arcs) - THIS SHOULD BE TESTED FOR ON RETURN!
 */

void
gcode_bolt_holes_aabb (gcode_block_t *block, gcode_vec2d_t min, gcode_vec2d_t max)
{
  gcode_block_t *index_block;
  gcode_bolt_holes_t *bolt_holes;
  gcode_vec2d_t center;
  gfloat_t radius;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  radius = bolt_holes->hole_diameter / 2;

  min[0] = min[1] = 1;
  max[0] = max[1] = 0;

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->type == GCODE_TYPE_ARC)
    {
      gcode_arc_center (index_block, center, GCODE_GET);

      if ((min[0] > max[0]) || (min[1] > max[1]))                               // If bounds were inside-out (unset), accept the hole directly;
      {
        min[0] = center[0] - radius;
        max[0] = center[0] + radius;
        min[1] = center[1] - radius;
        max[1] = center[1] + radius;
      }
      else                                                                      // If bounds are already valid (set), look for holes not inside;
      {
        if (center[0] - radius < min[0])
          min[0] = center[0] - radius;

        if (center[0] + radius > max[0])
          max[0] = center[0] + radius;

        if (center[1] - radius < min[1])
          min[1] = center[1] - radius;

        if (center[1] + radius > max[1])
          max[1] = center[1] + radius;
      }
    }

    index_block = index_block->next;
  }
}

void
gcode_bolt_holes_move (gcode_block_t *block, gcode_vec2d_t delta)
{
  gcode_bolt_holes_t *bolt_holes;
  gcode_vec2d_t orgnl_pt, xform_pt;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  GCODE_MATH_VEC2D_COPY (orgnl_pt, bolt_holes->position);
  GCODE_MATH_TRANSLATE (xform_pt, orgnl_pt, delta);
  GCODE_MATH_VEC2D_COPY (bolt_holes->position, xform_pt);

  gcode_bolt_holes_rebuild (block);
}

void
gcode_bolt_holes_spin (gcode_block_t *block, gcode_vec2d_t datum, gfloat_t angle)
{
  gcode_bolt_holes_t *bolt_holes;
  gcode_vec2d_t orgnl_pt, xform_pt;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  GCODE_MATH_VEC2D_SUB (orgnl_pt, bolt_holes->position, datum);
  GCODE_MATH_ROTATE (xform_pt, orgnl_pt, angle);
  GCODE_MATH_VEC2D_ADD (bolt_holes->position, xform_pt, datum);

  bolt_holes->offset_angle += angle;
  GCODE_MATH_WRAP_TO_360_DEGREES (bolt_holes->offset_angle);

  gcode_bolt_holes_rebuild (block);
}

void
gcode_bolt_holes_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_bolt_holes_t *bolt_holes, *model_bolt_holes;
  gcode_block_t *index_block, *new_block, *last_block;

  model_bolt_holes = (gcode_bolt_holes_t *)model->pdata;

  gcode_bolt_holes_init (block, gcode, model->parent);

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  bolt_holes = (gcode_bolt_holes_t *)(*block)->pdata;

  bolt_holes->position[0] = model_bolt_holes->position[0];
  bolt_holes->position[1] = model_bolt_holes->position[1];
  bolt_holes->number[0] = model_bolt_holes->number[0];
  bolt_holes->number[1] = model_bolt_holes->number[1];
  bolt_holes->type = model_bolt_holes->type;
  bolt_holes->hole_diameter = model_bolt_holes->hole_diameter;
  bolt_holes->offset_distance = model_bolt_holes->offset_distance;
  bolt_holes->offset_angle = model_bolt_holes->offset_angle;
  bolt_holes->offset = model_bolt_holes->offset;

  model->extruder->clone (&new_block, gcode, model->extruder);                  // Acquire a clone of the model's extruder, referenced as 'new_block'

  gcode_attach_as_extruder (*block, new_block);                                 // Attach the brand new extrusion clone as the extruder of this block

  gcode_bolt_holes_rebuild (*block);                                            // Copying an 'auto-built' list is pointless. We should 'rebuild' ours instead.
}

void
gcode_bolt_holes_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_bolt_holes_t *bolt_holes;
  gcode_block_t *index_block;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  bolt_holes->position[0] *= scale;
  bolt_holes->position[1] *= scale;
  bolt_holes->hole_diameter *= scale;
  bolt_holes->offset_distance *= scale;

  block->extruder->scale (block->extruder, scale);

  index_block = block->listhead;

  while (index_block)
  {
    if (index_block->scale)
      index_block->scale (index_block, scale);

    index_block = index_block->next;
  }
}

/**
 * Build the holes of the bolt_holes block 'block' as a list of 360 degree arcs
 * - one for each hole - attached to the listhead of 'block'. Any existing list
 * attached to the listhead gets freed first (hence the REbuild part).
 * NOTE: the arc blocks themselves are not being exposed as visible children of
 * 'block' in the GUI tree view, therefore rather unusually they get 'renamed'
 * to the name of 'block' so as to ensure that's what they pass on to the arcs
 * drawn in the GUI, making possible looking up 'block' if they get clicked on;
 */

void
gcode_bolt_holes_rebuild (gcode_block_t *block)
{
  gcode_bolt_holes_t *bolt_holes;
  gcode_block_t *new_block;
  gcode_arc_t *arc;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  gcode_list_free (&block->listhead);                                           // Walk the list and free;

  if (bolt_holes->type == GCODE_BOLT_HOLES_TYPE_RADIAL)
  {
    int i;

    for (i = 0; i < bolt_holes->number[0]; i++)                                 // Rebuild the list and calculate values for each arc;
    {
      gfloat_t angle;

      gcode_arc_init (&new_block, block->gcode, block);

      new_block->name = block->name;                                            // Reset each arc's 'name' to point not to itself but to its parent;

      arc = (gcode_arc_t *)new_block->pdata;

      angle = bolt_holes->offset_angle + 360.0 * ((gfloat_t)i) / ((gfloat_t)bolt_holes->number[0]);

      arc->radius = bolt_holes->hole_diameter * 0.5;
      arc->p[0] = bolt_holes->position[0] + bolt_holes->offset_distance * cos (angle * GCODE_DEG2RAD) - arc->radius;
      arc->p[1] = bolt_holes->position[1] + bolt_holes->offset_distance * sin (angle * GCODE_DEG2RAD);
      arc->start_angle = 180.0;
      arc->sweep_angle = 360.0;

      gcode_append_as_listtail (block, new_block);                              // Append 'new_block' to the end of 'block's list (as head if the list is NULL)
    }
  }
  else if (bolt_holes->type == GCODE_BOLT_HOLES_TYPE_MATRIX)
  {
    int i, j;

    for (i = 0; i < bolt_holes->number[0]; i++)                                 // Rebuild the list and calculate values for each arc;
    {
      for (j = 0; j < bolt_holes->number[1]; j++)
      {
        gcode_arc_init (&new_block, block->gcode, block);

        new_block->name = block->name;                                          // Reset each arc's 'name' to point not to itself but to its parent;

        arc = (gcode_arc_t *)new_block->pdata;

        arc->radius = bolt_holes->hole_diameter * 0.5;
        arc->p[0] = bolt_holes->position[0] + ((gfloat_t)i) * bolt_holes->offset_distance - arc->radius;
        arc->p[1] = bolt_holes->position[1] + (i % 2 ? (gfloat_t)(bolt_holes->number[1] - j - 1) : (gfloat_t)j) * bolt_holes->offset_distance;
        arc->start_angle = 180.0;
        arc->sweep_angle = 360.0;

        gcode_append_as_listtail (block, new_block);                            // Append 'new_block' to the end of 'block's list (as head if the list is NULL)
      }
    }
  }
}
