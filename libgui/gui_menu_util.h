/**
 *  gui_menu_util.h
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

#ifndef _GUI_MENU_UTIL_H
#define _GUI_MENU_UTIL_H

#include "gui.h"
#include <gtk/gtk.h>

void base_unit_changed_callback (GtkWidget *widget, gpointer data);
void update_progress (void *gui, gfloat_t progress);
void generic_dialog (void *gui, char *message);
void generic_error (void *gui, char *message);
GtkTreeIter gui_insert_under_iter (gui_t *gui, GtkTreeIter *iter, gcode_block_t *block);
GtkTreeIter gui_append_under_iter (gui_t *gui, GtkTreeIter *iter, gcode_block_t *block);
GtkTreeIter gui_insert_after_iter (gui_t *gui, GtkTreeIter *iter, gcode_block_t *block);
GtkTreeIter insert_primitive (gui_t *gui, gcode_block_t *block, gcode_block_t *selected_block, GtkTreeIter *iter, int action);
void remove_primitive (gui_t *gui, gcode_block_t *block, GtkTreeIter *iter);
void remove_and_destroy_primitive (gui_t *gui, gcode_block_t *block, GtkTreeIter *iter);
void gui_recreate_subtree_of (gui_t *gui, GtkTreeIter *iter);
void gui_recreate_whole_tree (gui_t *gui);
void gui_renumber_subtree_of (gui_t *gui, GtkTreeIter *iter);
void gui_renumber_whole_tree (gui_t *gui);
void get_selected_block (gui_t *gui, gcode_block_t **selected_block, GtkTreeIter *iter);
void set_selected_row_with_iter (gui_t *gui, GtkTreeIter *iter);
void set_selected_row_with_block (gui_t *gui, gcode_block_t *block);
void update_project_modified_flag (gui_t *gui, uint8_t modified);
void update_menu_by_project_state (gui_t *gui, uint8_t state);
void update_menu_by_selected_item (gui_t *gui, gcode_block_t *selected_block);
void gui_show_project (gui_t *gui);
void gui_destroy (void);

#endif
