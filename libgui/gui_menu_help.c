/**
 *  gui_menu_help.c
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

#include "gui_menu_help.h"
#include "gui_menu_util.h"
#include "gui.h"

void
gui_menu_help_manual_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;

  gui = (gui_t *)data;

  generic_dialog (gui, "\nVisit http://gcam.js.cx and click on Manual.\n");
}

void
gui_menu_help_about_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  char message[512];

  gui = (gui_t *)data;

  /* *INDENT-OFF* */

  sprintf (message, "GCAM Special Edition v%s\n( package build date %s )\n\n"
                    "AUTHOR: Justin Shumaker\n"
                    "WEBSITE: http://gcam.js.cx\n"
                    "EMAIL: justin@js.cx\n\n"
                    "EDITOR: Asztalos Attila Oszk\u00E1r\n"
                    "GIT: http://github.com/blinkenlight/gcam",
                    VERSION, BUILD_DATE);

  /* *INDENT-ON* */

  generic_dialog (gui, message);
}
