/**
 *  gcode_code.c
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

#include "gcode_code.h"

void
gcode_code_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_CODE, GCODE_FLAGS_LOCK);

  (*block)->free = gcode_code_free;
  (*block)->save = gcode_code_save;
  (*block)->load = gcode_code_load;
  (*block)->make = gcode_code_make;

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Manual G-Code Entry");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));
}

void
gcode_code_free (gcode_block_t **block)
{
  free (*block);
  *block = NULL;
}

void
gcode_code_save (gcode_block_t *block, FILE *fh)
{
}

void
gcode_code_load (gcode_block_t *block, FILE *fh)
{
}

void
gcode_code_make (gcode_block_t *block)
{
  GCODE_CLEAR (block);

  GCODE_COMMENT (block, "Insert Custom G-Code Here");
}
