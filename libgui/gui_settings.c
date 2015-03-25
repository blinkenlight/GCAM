/**
 *  gui_settings.c
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

#include "gui_settings.h"
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <expat.h>

#ifdef WIN32
#include <windows.h>
#endif

void
gui_settings_init (gui_settings_t *settings)
{
  settings->voxel_resolution = 250;
}

void
gui_settings_free (gui_settings_t *settings)
{
}

static void
start (void *data, const char *xmlelem, const char **xmlattr)
{
  gui_settings_t *settings;
  char tag[256], name[256];
  char *value;
  int i;

  settings = (gui_settings_t *)data;

  strncpy (tag, xmlelem, sizeof (tag));

  tag[sizeof (tag) - 1] = '\0';

  strswp (tag, '_', '-');

  if (strcmp (tag, GCODE_XML_TAG_SETTING) == 0)
  {
    for (i = 0; xmlattr[i]; i += 2)
    {
      strncpy (name, xmlattr[i], sizeof (name));

      name[sizeof (name) - 1] = '\0';

      strswp (name, '_', '-');

      value = (char *)xmlattr[i + 1];

      if (strcmp (name, GCODE_XML_ATTR_SETTING_VOXEL_RESOLUTION) == 0)
      {
        settings->voxel_resolution = atoi (value);
      }
    }
  }
}

static void
end (void *data, const char *xmlelem)
{
}

int
gui_settings_read (gui_settings_t *settings)
{
  FILE *fh = NULL;
  int length, nomore;
  char fullpath[256], *filename, *buffer;

  filename = (char *)GCODE_XML_SETTINGS_FILENAME;

  XML_Parser parser = XML_ParserCreate ("UTF-8");

  if (!parser)
  {
    REMARK ("Failed to allocate memory for XML parser\n");
    return (1);
  }

  XML_SetElementHandler (parser, start, end);
  XML_SetUserData (parser, settings);

  /* Open and read the file 'settings.xml' */

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
