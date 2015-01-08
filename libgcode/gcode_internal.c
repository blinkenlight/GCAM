/**
 *  gcode_internal.c
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

#include "gcode_internal.h"

void
gcode_internal_init (gcode_block_t *block, gcode_t *gcode, gcode_block_t *parent, uint8_t type, uint8_t flags)
{
  /**
   * The name is a unique identifier based on the pointer address.
   * lowest 3-bits are always 0, so we can divide by 8
   * OpenGL only allows 32-bits for names, so condense the address as much as possible.
   * In the long term this may need to become a lookup table.
   */

  block->type = type;
  block->flags = flags;
  block->name = (uint32_t)((intptr_t)block - (intptr_t)gcode) >> 3;
  block->gcode = gcode;
  block->parent = parent;
  block->listhead = NULL;
  block->extruder = NULL;
  block->prev = NULL;
  block->next = NULL;
  block->offref = NULL;
  block->offset = NULL;
  block->pdata = NULL;
  block->code = NULL;

  block->free = NULL;
  block->make = NULL;
  block->save = NULL;
  block->load = NULL;
  block->ends = NULL;
  block->draw = NULL;
  block->eval = NULL;
  block->length = NULL;
  block->clone = NULL;
  block->scale = NULL;
  block->aabb = NULL;
}

static void
format_z (char *format, char **format2, unsigned int number)
{
  unsigned int i, j, s;

  if (number > 9)
    return;

  s = strlen (format) + 1;
  *format2 = (char *)malloc (2 * s);
  memcpy (*format2, format, s);

  j = 0;

  for (i = 0; i < s - 1; i++)
  {
    if (format[i] == '%' && format[i + 1] == 'z')
    {
      /* copy % */
      (*format2)[j] = format[i];
      j++;
      i += 1;

      (*format2)[j] = '.';
      j++;

      sprintf (&(*format2)[j], "%d", number);
      j++;

      (*format2)[j] = 'f';
      j++;
    }
    else
    {
      (*format2)[j] = format[i];
      j++;
    }
  }

  (*format2)[j] = 0;
}

void
gsprintf (char *target, unsigned int number, char *format, ...)
{
  va_list arglist;
  char *format2;

  va_start (arglist, format);

  format2 = NULL;

  format_z (format, &format2, number);
  vsprintf (target, format2, arglist);

  free (format2);

  va_end (arglist);
}

void
strswp (char *target, char oldchar, char newchar)
{
  char *index;

  for (index = target; *index; index++)
  {
    if (*index == oldchar)
    {
      *index = newchar;
    }
  }
}
