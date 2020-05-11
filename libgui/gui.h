/**
 *  gui.h
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

#ifndef _GUI_H
#define _GUI_H

#include "gui_opengl.h"
#include "gui_settings.h"
#include "gui_endmills.h"
#include "gui_machines.h"
#include <inttypes.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

static const char *GCAM_FATAL_SETTINGS = "\nFailed to load the list of settings - GCAM will now close...\n";
static const char *GCAM_FATAL_MACHINES = "\nFailed to load the list of machines - GCAM will now close...\n";
static const char *GCAM_FATAL_ENDMILLS = "\nFailed to load the list of endmills - GCAM will now close...\n";

typedef struct gui_s
{
  gcode_t gcode;
  gui_opengl_t opengl;
  gui_settings_t settings;

  gui_machine_list_t machines;
  gui_endmill_list_t endmills;

  GTimer *timer;
  double event_time;

  GtkWidget *window;
  GtkUIManager *ui_manager;
  GtkWidget *panel_vbox;
  GtkWidget *panel_tab_vbox;

  int project_state;
  int16_t mouse_x;
  int16_t mouse_y;

  GtkTreeStore *gcode_block_store;
  GtkWidget *gcode_block_treeview;
  GtkCellRenderer *comment_cell;
  GtkTreePath *row_drop_path;
  GtkTreeViewDropPosition row_drop_spot;

  GtkWidget *progress_bar;
  gcode_block_t *selected_block;

  char title[64];
  char filename[256];
  int modified;

  char current_folder[256];

  int ignore_signals;

  int first_render;
} gui_t;

void gui_init (char *filename);
void gui_attach (gcode_t *gcode, gui_t *gui);

#endif
