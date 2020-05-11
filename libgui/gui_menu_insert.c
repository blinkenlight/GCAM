/**
 *  gui_menu_insert.c
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

#include "gui_menu_insert.h"
#include "gcode.h"
#include "gui.h"
#include "gui_menu_util.h"
#include "gui_define.h"

void
gui_menu_insert_tool_change_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  gcode_tool_t *tool;
  gui_endmill_t *endmill;
  GtkTreeIter selected_iter;
  gui_t *gui;

  gui = (gui_t *)data;

  gcode_tool_init (&new_block, &gui->gcode, NULL);

  tool = (gcode_tool_t *)new_block->pdata;

  endmill = &gui->endmills.endmill[0];

  strcpy (tool->label, endmill->description);
  tool->diameter = gui_endmills_size (endmill, gui->gcode.units);
  tool->number = endmill->number;

  get_selected_block (gui, &selected_block, &selected_iter);

  insert_primitive (gui, new_block, selected_block, &selected_iter, GUI_INSERT_AFTER);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_template_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  GtkTreeIter selected_iter;
  gui_t *gui;

  gui = (gui_t *)data;

  gcode_template_init (&new_block, &gui->gcode, NULL);

  get_selected_block (gui, &selected_block, &selected_iter);

  insert_primitive (gui, new_block, selected_block, &selected_iter, GUI_INSERT_AFTER);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_sketch_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  GtkTreeIter selected_iter;
  gui_t *gui;

  gui = (gui_t *)data;

  gcode_sketch_init (&new_block, &gui->gcode, NULL);

  get_selected_block (gui, &selected_block, &selected_iter);

  insert_primitive (gui, new_block, selected_block, &selected_iter, GUI_INSERT_AFTER);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_arc_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  GtkTreeIter selected_iter;
  gui_t *gui;
  int insert_spot;

  gui = (gui_t *)data;

  gcode_arc_init (&new_block, &gui->gcode, NULL);

  get_selected_block (gui, &selected_block, &selected_iter);

  if ((selected_block->type == GCODE_TYPE_SKETCH) || (selected_block->type == GCODE_TYPE_EXTRUSION))
    insert_spot = GUI_INSERT_UNDER;
  else
    insert_spot = GUI_INSERT_AFTER;

  insert_primitive (gui, new_block, selected_block, &selected_iter, insert_spot | GUI_INSERT_WITH_TANGENCY);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_line_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  GtkTreeIter selected_iter;
  gui_t *gui;
  int insert_spot;

  gui = (gui_t *)data;

  gcode_line_init (&new_block, &gui->gcode, NULL);

  get_selected_block (gui, &selected_block, &selected_iter);

  if (selected_block->type == GCODE_TYPE_SKETCH || selected_block->type == GCODE_TYPE_EXTRUSION)
    insert_spot = GUI_INSERT_UNDER;
  else
    insert_spot = GUI_INSERT_AFTER;

  insert_primitive (gui, new_block, selected_block, &selected_iter, insert_spot | GUI_INSERT_WITH_TANGENCY);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_bolt_holes_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  gcode_bolt_holes_t *bolt_holes;
  gcode_tool_t *tool;
  GtkTreeIter selected_iter;
  gui_t *gui;

  gui = (gui_t *)data;

  gcode_bolt_holes_init (&new_block, &gui->gcode, NULL);

  get_selected_block (gui, &selected_block, &selected_iter);

  /* Assign the bolt holes diameter to the current tool diameter */
  tool = gcode_tool_find (selected_block);

  bolt_holes = (gcode_bolt_holes_t *)new_block->pdata;

  bolt_holes->hole_diameter = tool->diameter;

  gcode_bolt_holes_rebuild (new_block);

  insert_primitive (gui, new_block, selected_block, &selected_iter, GUI_INSERT_AFTER);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_drill_holes_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  GtkTreeIter selected_iter;
  gui_t *gui;

  gui = (gui_t *)data;

  gcode_drill_holes_init (&new_block, &gui->gcode, NULL);

  get_selected_block (gui, &selected_block, &selected_iter);

  insert_primitive (gui, new_block, selected_block, &selected_iter, GUI_INSERT_AFTER);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_point_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gcode_block_t *new_block, *selected_block;
  GtkTreeIter selected_iter;
  gui_t *gui;
  int insert_spot;

  gui = (gui_t *)data;

  gcode_point_init (&new_block, &gui->gcode, NULL);

  get_selected_block (gui, &selected_block, &selected_iter);

  if (selected_block->type == GCODE_TYPE_DRILL_HOLES)
    insert_spot = GUI_INSERT_UNDER;
  else
    insert_spot = GUI_INSERT_AFTER;

  insert_primitive (gui, new_block, selected_block, &selected_iter, insert_spot);

  update_project_modified_flag (gui, 1);
}

void
gui_menu_insert_image_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gui_t *gui;

  gui = (gui_t *)data;

  dialog = gtk_file_chooser_dialog_new ("Select Image",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "PNG (*.png)");
  gtk_file_filter_add_pattern (filter, "*.png");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All files (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    gcode_block_t *selected_block, *new_block;
    GtkTreeIter selected_iter;
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    get_selected_block (gui, &selected_block, &selected_iter);

    /* Create Image Block */
    gcode_image_init (&new_block, &gui->gcode, selected_block);

    gcode_image_open (new_block, filename);

    insert_primitive (gui, new_block, selected_block, &selected_iter, GUI_INSERT_AFTER);

    g_free (filename);

    gui_opengl_build_gridxy_display_list (&gui->opengl);
    gui_opengl_build_gridxz_display_list (&gui->opengl);

    gcode_prep (&gui->gcode);

    gui->opengl.rebuild_view_display_list = 1;
    gui_opengl_context_redraw (&gui->opengl, selected_block);
  }

  gtk_widget_destroy (dialog);
}
