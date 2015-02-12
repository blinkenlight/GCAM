/**
 *  gui_machines.c
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

#include "gui_machines.h"
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <expat.h>

#ifdef WIN32
#include <windows.h>
#endif

void
gui_machines_init (gui_machine_list_t *machine_list)
{
  machine_list->number = 0;
  machine_list->machine = NULL;
}

void
gui_machines_free (gui_machine_list_t *machine_list)
{
  machine_list->number = 0;
  free (machine_list->machine);
  machine_list->machine = NULL;
}

static void
start (void *data, const char *xmlelem, const char **xmlattr)
{
  gui_machine_list_t *machine_list;
  char tag[256], name[256];
  char *value;
  int i;

  machine_list = (gui_machine_list_t *)data;

  strcpy (tag, xmlelem);
  strswp (tag, '_', '-');

  if (strcmp (tag, GCODE_XML_TAG_MACHINE) == 0)
  {
    machine_list->machine = realloc (machine_list->machine, (machine_list->number + 1) * sizeof (gui_machine_t));
    strcpy (machine_list->machine[machine_list->number].name, "");

    machine_list->machine[machine_list->number].travel[0] = 0.0;
    machine_list->machine[machine_list->number].travel[1] = 0.0;
    machine_list->machine[machine_list->number].travel[2] = 0.0;

    machine_list->machine[machine_list->number].maxipm[0] = 0.0;
    machine_list->machine[machine_list->number].maxipm[1] = 0.0;
    machine_list->machine[machine_list->number].maxipm[2] = 0.0;

    machine_list->machine[machine_list->number].options = 0;

    for (i = 0; xmlattr[i]; i += 2)
    {
      strcpy (name, xmlattr[i]);
      strswp (name, '_', '-');

      value = (char *)xmlattr[i + 1];

      if (strcmp (name, GCODE_XML_ATTR_MACHINE_NAME) == 0)
      {
        strcpy (machine_list->machine[machine_list->number].name, value);
      }
    }
  }

  if ((strcmp (tag, GCODE_XML_TAG_MACHINE_SETTING) == 0) ||
      (strcmp (tag, GCODE_XML_TAG_MACHINE_PROPERTY) == 0))
  {
    for (i = 0; xmlattr[i]; i += 2)
    {
      strcpy (name, xmlattr[i]);
      strswp (name, '_', '-');

      value = (char *)xmlattr[i + 1];

      if (strcmp (name, GCODE_XML_ATTR_PROPERTY_TRAVEL_X) == 0)
      {
        machine_list->machine[machine_list->number].travel[0] = atof (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_TRAVEL_Y) == 0)
      {
        machine_list->machine[machine_list->number].travel[1] = atof (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_TRAVEL_Z) == 0)
      {
        machine_list->machine[machine_list->number].travel[2] = atof (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_MAX_IPM_X) == 0)
      {
        machine_list->machine[machine_list->number].maxipm[0] = atof (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_MAX_IPM_Y) == 0)
      {
        machine_list->machine[machine_list->number].maxipm[1] = atof (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_MAX_IPM_Z) == 0)
      {
        machine_list->machine[machine_list->number].maxipm[2] = atof (value);
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_SPINDLE_CONTROL) == 0)
      {
        if (strcmp (value, GCODE_XML_VAL_PROPERTY_YES) == 0)
          machine_list->machine[machine_list->number].options |= GCODE_MACHINE_OPTION_SPINDLE_CONTROL;
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_TOOL_CHANGE) == 0)
      {
        if (strcmp (value, GCODE_XML_VAL_PROPERTY_AUTO) == 0)
          machine_list->machine[machine_list->number].options |= GCODE_MACHINE_OPTION_AUTOMATIC_TOOL_CHANGE;
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_HOME_SWITCHES) == 0)
      {
        if (strcmp (value, GCODE_XML_VAL_PROPERTY_YES) == 0)
          machine_list->machine[machine_list->number].options |= GCODE_MACHINE_OPTION_HOME_SWITCHES;
      }
      else if (strcmp (name, GCODE_XML_ATTR_PROPERTY_COOLANT) == 0)
      {
        if (strcmp (value, GCODE_XML_VAL_PROPERTY_YES) == 0)
          machine_list->machine[machine_list->number].options |= GCODE_MACHINE_OPTION_COOLANT;
      }
    }
  }
}

static void
end (void *data, const char *xmlelem)
{
  gui_machine_list_t *machine_list;
  char tag[256];

  machine_list = (gui_machine_list_t *)data;

  strcpy (tag, xmlelem);
  strswp (tag, '_', '-');

  if (strcmp (tag, GCODE_XML_TAG_MACHINE) == 0)
    machine_list->number++;
}

int
gui_machines_read (gui_machine_list_t *machine_list)
{
  FILE *fh = NULL;
  int length, nomore;
  char fullpath[256], *filename, *buffer;

  filename = (char *)GCODE_XML_MACHINES_FILENAME;

  XML_Parser parser = XML_ParserCreate ("UTF-8");

  machine_list->number = 0;

  if (!parser)
  {
    REMARK ("Failed to allocate memory for XML parser\n");
    return (1);
  }

  XML_SetElementHandler (parser, start, end);
  XML_SetUserData (parser, machine_list);

  /* Open and read the file 'machines.xml' */

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
  buffer = (char *)malloc (length);
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

gui_machine_t *
gui_machines_find (gui_machine_list_t *machine_list, char *machine_name, uint8_t fallback)
{
  int i;

  for (i = 0; i < machine_list->number; i++)
  {
    if (strcmp (machine_name, machine_list->machine[i].name) == 0)
    {
      return (&machine_list->machine[i]);
    }
  }

  if (fallback)
    return (&machine_list->machine[0]);

  return (NULL);
}
