/**
 *  gui_menu_assistant.c
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

#include "gui_menu_assistant.h"
#include "gui.h"
#include "gui_define.h"
#include "gui_menu_util.h"
#include "gcode.h"

/* NOT AN EXACT CONVERSION - uses x25 for round values */
#define SCALED_INCHES(x) (GCODE_UNITS ((&gui->gcode), x))

static void
polygon_on_assistant_close_cancel (GtkWidget *assistant, gpointer data)
{
  GtkWidget **wlist;

  wlist = (GtkWidget **)data;

  gtk_widget_destroy (assistant);

  free (wlist);
}

static void
polygon_on_assistant_apply (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  GtkTreeIter selected_iter;
  gcode_block_t *selected_block, *sketch_block, *line_block;
  gcode_line_t *line;
  gfloat_t sides, radius, angle;
  int i, si;

  wlist = (GtkWidget **)data;                                                   // Retrieve a reference to the GUI context;

  gui = (gui_t *)wlist[0];                                                      // Using that, retrieve a reference to 'gui';

  get_selected_block (gui, &selected_block, &selected_iter);

  sides = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[1]));
  radius = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));

  /* Create Sketch */
  gcode_sketch_init (&sketch_block, &gui->gcode, selected_block);

  /* Create Lines */
  si = (int)sides;

  for (i = 0; i < si; i++)
  {
    gcode_line_init (&line_block, &gui->gcode, sketch_block);
    line = (gcode_line_t *)line_block->pdata;

    line->p0[0] = 0.0;
    line->p0[1] = radius;
    angle = 360.0 * ((float)i) / ((float)sides);
    GCODE_MATH_ROTATE (line->p0, line->p0, angle);

    line->p1[0] = 0.0;
    line->p1[1] = radius;
    angle = 360.0 * ((float)(i + 1)) / ((float)sides);
    GCODE_MATH_ROTATE (line->p1, line->p1, angle);

    gcode_append_as_listtail (sketch_block, line_block);
  }

  insert_primitive (gui, sketch_block, selected_block, &selected_iter, GUI_INSERT_AFTER);

  /* XXX - this should technically not be there */
  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, sketch_block);

  update_project_modified_flag (gui, 1);
}

static void
polygon_create_page1 (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *sides_spin;
  GtkWidget *radius_spin;
  GtkWidget **wlist;
  GdkPixbuf *pixbuf;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  table = gtk_table_new (2, 2, TRUE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), BORDER_WIDTH);

  label = gtk_label_new ("Sides");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

  sides_spin = gtk_spin_button_new_with_range (3, 20, 1);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sides_spin), 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sides_spin), 6);
  gtk_table_attach_defaults (GTK_TABLE (table), sides_spin, 1, 2, 0, 1);

  g_signal_connect_swapped (sides_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  label = gtk_label_new ("Radius");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

  radius_spin = gtk_spin_button_new_with_range (GCODE_PRECISION, SCALED_INCHES (10.0), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (radius_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (radius_spin), SCALED_INCHES (0.5));
  gtk_table_attach_defaults (GTK_TABLE (table), radius_spin, 1, 2, 1, 2);

  g_signal_connect_swapped (radius_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  wlist[1] = sides_spin;
  wlist[2] = radius_spin;

  gtk_widget_show_all (table);

  gtk_assistant_append_page (GTK_ASSISTANT (assistant), table);
  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), table, "Polygon");
  gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), table, GTK_ASSISTANT_PAGE_CONFIRM);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), table, TRUE);

  pixbuf = gtk_widget_render_icon (assistant, GCAM_STOCK_ASSIST_POLYGON, GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_assistant_set_page_header_image (GTK_ASSISTANT (assistant), table, pixbuf);
  g_object_unref (pixbuf);
}

void
gui_menu_assistant_polygon_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget *assistant;
  GtkWidget **wlist;

  gui = (gui_t *)data;

  assistant = gtk_assistant_new ();

  gtk_window_set_title (GTK_WINDOW (assistant), "Polygon");
  gtk_window_set_default_size (GTK_WINDOW (assistant), -1, -1);
  gtk_window_set_screen (GTK_WINDOW (assistant), gtk_widget_get_screen (gui->window));
  gtk_window_set_transient_for (GTK_WINDOW (assistant), GTK_WINDOW (gui->window));

  /* Setup Global Widgets */
  wlist = malloc (3 * sizeof (GtkWidget *));

  wlist[0] = (void *)gui;

  polygon_create_page1 (assistant, wlist);

  g_signal_connect (assistant, "cancel", G_CALLBACK (polygon_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "close", G_CALLBACK (polygon_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "apply", G_CALLBACK (polygon_on_assistant_apply), wlist);

  gtk_widget_show (assistant);
}
