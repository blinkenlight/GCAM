/**
 *  gui_endmills.c
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

#include "gui_endmills.h"
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <expat.h>

#ifdef WIN32
#include <windows.h>
#endif

void
gui_endmills_init (gui_endmill_list_t *endmill_list)
{
  endmill_list->number = 0;
  endmill_list->endmill = NULL;
}

void
gui_endmills_free (gui_endmill_list_t *endmill_list)
{
  endmill_list->number = 0;
  free (endmill_list->endmill);
  endmill_list->endmill = NULL;
}

static void
start (void *data, const char *xmlelem, const char **xmlattr)
{
  gui_endmill_list_t *endmill_list;
  gui_endmill_t *new_endmill;
  char tag[256], name[256];
  char *value;
  int i;

  endmill_list = (gui_endmill_list_t *)data;

  strncpy (tag, xmlelem, sizeof (tag));

  tag[sizeof (tag) - 1] = '\0';

  strswp (tag, '_', '-');

  if (strcmp (tag, GCODE_XML_TAG_ENDMILL) == 0)
  {
    endmill_list->endmill = realloc (endmill_list->endmill, (endmill_list->number + 1) * sizeof (gui_endmill_t));

    new_endmill = &endmill_list->endmill[endmill_list->number];

    strcpy (new_endmill->description, "");

    for (i = 0; xmlattr[i]; i += 2)
    {
      strncpy (name, xmlattr[i], sizeof (name));

      name[sizeof (name) - 1] = '\0';

      strswp (name, '_', '-');

      value = (char *)xmlattr[i + 1];

      if (strcmp (name, GCODE_XML_ATTR_ENDMILL_NUMBER) == 0)
      {
        new_endmill->number = (uint8_t)atoi (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_ENDMILL_DIAMETER) == 0)
      {
        new_endmill->diameter = atof (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_ENDMILL_UNIT) == 0)
      {
        if (strcmp (value, GCODE_XML_VAL_ENDMILL_UNIT_INCH) == 0)
        {
          new_endmill->unit = GCODE_UNITS_INCH;
        }
        else if (strcmp (value, GCODE_XML_VAL_ENDMILL_UNIT_MILLIMETER) == 0)
        {
          new_endmill->unit = GCODE_UNITS_MILLIMETER;
        }
      }
      else if (strcmp (name, GCODE_XML_ATTR_ENDMILL_DESCRIPTION) == 0)
      {
        strncpy (new_endmill->description, value, sizeof (new_endmill->description));

        new_endmill->description[sizeof (new_endmill->description) - 1] = '\0';
      }
    }

    endmill_list->number++;
  }
}

static void
end (void *data, const char *xmlelem)
{
}

int
gui_endmills_read (gui_endmill_list_t *endmill_list)
{
  FILE *fh = NULL;
  int length, nomore, i;
  char fullpath[256], *filename, *buffer;

  filename = (char *)GCODE_XML_ENDMILLS_FILENAME;

  XML_Parser parser = XML_ParserCreate ("UTF-8");

  endmill_list->number = 0;

  if (!parser)
  {
    REMARK ("Failed to allocate memory for XML parser\n");
    return (1);
  }

  XML_SetElementHandler (parser, start, end);
  XML_SetUserData (parser, endmill_list);

  /* Open and read the file 'endmills.xml' */

#ifdef WIN32
  GetModuleFileName (NULL, fullpath, 230);                                      // Try to open from where the executable runs;
  sprintf (fullpath, "%s\\share\\%s", dirname (fullpath), filename);
#else
  sprintf (fullpath, "%s%s", SHARE_PREFIX, filename);                           // Try to open from formal Linux install path;
#endif

  fh = fopen (fullpath, "r");

  if (!fh)                                                                      // Try to open from current working directory;
  {
    getcwd (fullpath, 230);

#ifdef WIN32
    sprintf (fullpath, "%s\\share\\%s", fullpath, filename);                    // Rather astonishingly this isn't actually necessary: forward slashes work ok,
#else
    sprintf (fullpath, "%s/share/%s", fullpath, filename);                      // even in WIN32; but that doesn't mean we shouldn't act in a civilized manner.
#endif

    fh = fopen (fullpath, "r");
  }

  if (!fh)                                                                      // Shiver me timbers! Plan C - proceed to panic at flank speed, aaaarrrrrgh...!
  {
    REMARK ("Failed to open file '%s'\n", filename);
    XML_ParserFree (parser);
    return (1);
  }

  fseek (fh, 0, SEEK_END);
  length = ftell (fh);
  buffer = malloc (length);
  fseek (fh, 0, SEEK_SET);
  nomore = fread (buffer, 1, length, fh);

  if (XML_Parse (parser, buffer, nomore, 1) == XML_STATUS_ERROR)
  {
    REMARK ("XML parse error in file '%s' at line %d: %s\n", filename, (int)XML_GetCurrentLineNumber (parser), XML_ErrorString (XML_GetErrorCode (parser)));
    XML_ParserFree (parser);
    free (buffer);
    fclose (fh);
    return (1);
  }

  XML_ParserFree (parser);
  free (buffer);
  fclose (fh);

  return (0);
}

gfloat_t
gui_endmills_size (gui_endmill_t *endmill, uint8_t unit)
{
  if ((endmill->unit == GCODE_UNITS_INCH) && (unit == GCODE_UNITS_MILLIMETER))
    return (endmill->diameter * GCODE_INCH2MM);

  if ((endmill->unit == GCODE_UNITS_MILLIMETER) && (unit == GCODE_UNITS_INCH))
    return (endmill->diameter * GCODE_MM2INCH);

  return (endmill->diameter);
}

gui_endmill_t *
gui_endmills_find (gui_endmill_list_t *endmill_list, char *endmill_name, uint8_t fallback)
{
  int i;

  for (i = 0; i < endmill_list->number; i++)
  {
    if (strcmp (endmill_name, endmill_list->endmill[i].description) == 0)
    {
      return (&endmill_list->endmill[i]);
    }
  }

  if (fallback)
    return (&endmill_list->endmill[0]);

  return (NULL);
}
