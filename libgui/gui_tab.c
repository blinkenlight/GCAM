/**
 *  gui_tab.c
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

#include "gui_tab.h"
#include "gui_define.h"
#include "gui_endmills.h"
#include "gui_menu_util.h"
#include <unistd.h>

/* NOT AN EXACT CONVERSION - uses x25 for round values */
#define SCALED_INCHES(x) (GCODE_UNITS (gcode, x))

static void
generic_destroy_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;

  wlist = (GtkWidget **)data;
  free (wlist);
}

static void
begin_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_begin_t *begin;
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  begin = (gcode_begin_t *)block->pdata;

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));

  if (strcmp (text_field, "None") == 0)
  {
    begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_NONE;
  }
  else if (strcmp (text_field, "Workspace 1 (G54)") == 0)
  {
    begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE1;
  }
  else if (strcmp (text_field, "Workspace 2 (G55)") == 0)
  {
    begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE2;
  }
  else if (strcmp (text_field, "Workspace 3 (G56)") == 0)
  {
    begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE3;
  }
  else if (strcmp (text_field, "Workspace 4 (G57)") == 0)
  {
    begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE4;
  }
  else if (strcmp (text_field, "Workspace 5 (G58)") == 0)
  {
    begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE5;
  }
  else if (strcmp (text_field, "Workspace 6 (G59)") == 0)
  {
    begin->coordinate_system = GCODE_BEGIN_COORDINATE_SYSTEM_WORKSPACE6;
  }

  g_free (text_field);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_begin (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *begin_tab;
  GtkWidget *alignment;
  GtkWidget *begin_table;
  GtkWidget *label;
  GtkWidget *cs_combo;
  gcode_begin_t *begin;
  uint16_t row;

  /**
   * Begin Parameters
   */

  begin = (gcode_begin_t *)block->pdata;

  wlist = (GtkWidget **)malloc (3 * sizeof (GtkWidget *));
  row = 0;

  begin_tab = gtk_frame_new ("Begin Parameters");
  g_signal_connect (begin_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), begin_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (begin_tab), alignment);

  begin_table = gtk_table_new (1, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (begin_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (begin_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (begin_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), begin_table);

  label = gtk_label_new ("Coord System");
  gtk_table_attach_defaults (GTK_TABLE (begin_table), label, 0, 1, row, row + 1);

  cs_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (cs_combo), "None");
  gtk_combo_box_append_text (GTK_COMBO_BOX (cs_combo), "Workspace 1 (G54)");
  gtk_combo_box_append_text (GTK_COMBO_BOX (cs_combo), "Workspace 2 (G55)");
  gtk_combo_box_append_text (GTK_COMBO_BOX (cs_combo), "Workspace 3 (G56)");
  gtk_combo_box_append_text (GTK_COMBO_BOX (cs_combo), "Workspace 4 (G57)");
  gtk_combo_box_append_text (GTK_COMBO_BOX (cs_combo), "Workspace 5 (G58)");
  gtk_combo_box_append_text (GTK_COMBO_BOX (cs_combo), "Workspace 6 (G59)");
  gtk_combo_box_set_active (GTK_COMBO_BOX (cs_combo), begin->coordinate_system);
  g_signal_connect (cs_combo, "changed", G_CALLBACK (begin_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (begin_table), cs_combo, 1, 2, row, row + 1);
  row++;

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = cs_combo;
}

static void
end_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_end_t *end;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  end = (gcode_end_t *)block->pdata;

  end->retract_position[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  end->retract_position[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  end->retract_position[2] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[4]));

  end->home_all_axes = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[5]));

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_end (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *end_tab;
  GtkWidget *alignment;
  GtkWidget *end_table;
  GtkWidget *label;
  GtkWidget *posx_spin;
  GtkWidget *posy_spin;
  GtkWidget *posz_spin;
  GtkWidget *home_check_button;
  gcode_t *gcode;
  gcode_end_t *end;
  uint16_t row;

  /**
   * End Parameters
   */

  gcode = (gcode_t *)block->gcode;

  end = (gcode_end_t *)block->pdata;

  wlist = (GtkWidget **)malloc (6 * sizeof (GtkWidget *));
  row = 0;

  end_tab = gtk_frame_new ("End Parameters");
  g_signal_connect (end_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), end_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (end_tab), alignment);

  end_table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (end_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (end_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (end_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), end_table);

  label = gtk_label_new ("Retract To (X)");

  posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (20.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (posx_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (posx_spin), end->retract_position[0]);
  g_signal_connect (posx_spin, "value-changed", G_CALLBACK (end_update_callback), wlist);

  if ((gcode->machine_options & GCODE_MACHINE_OPTION_HOME_SWITCHES) == 0)
  {
    gtk_table_attach_defaults (GTK_TABLE (end_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (end_table), posx_spin, 1, 2, row, row + 1);
    row++;
  }

  label = gtk_label_new ("Retract To (Y)");

  posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (20.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (posy_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (posy_spin), end->retract_position[1]);
  g_signal_connect (posy_spin, "value-changed", G_CALLBACK (end_update_callback), wlist);

  if ((gcode->machine_options & GCODE_MACHINE_OPTION_HOME_SWITCHES) == 0)
  {
    gtk_table_attach_defaults (GTK_TABLE (end_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (end_table), posy_spin, 1, 2, row, row + 1);
    row++;
  }

  label = gtk_label_new ("Retract To (Z)");

  posz_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (3.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (posz_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (posz_spin), end->retract_position[2]);
  g_signal_connect (posz_spin, "value-changed", G_CALLBACK (end_update_callback), wlist);

  if ((gcode->machine_options & GCODE_MACHINE_OPTION_HOME_SWITCHES) == 0)
  {
    gtk_table_attach_defaults (GTK_TABLE (end_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (end_table), posz_spin, 1, 2, row, row + 1);
    row++;
  }

  label = gtk_label_new ("Home all Axes (G28)");

  home_check_button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (home_check_button), end->home_all_axes);
  g_signal_connect (home_check_button, "toggled", G_CALLBACK (end_update_callback), wlist);

  if (gcode->machine_options & GCODE_MACHINE_OPTION_HOME_SWITCHES)
  {
    gtk_table_attach_defaults (GTK_TABLE (end_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (end_table), home_check_button, 1, 2, row, row + 1);
    row++;
  }

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = posx_spin;
  wlist[3] = posy_spin;
  wlist[4] = posz_spin;
  wlist[5] = home_check_button;
}

static void
line_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_line_t *line;
  gfloat_t clength, nlength, cangle, nangle;
  gcode_vec2d_t p0, p1, vec;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  line = (gcode_line_t *)block->pdata;

  p0[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  p0[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  p1[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[4]));
  p1[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));

  /* If a value for the position changed then update them otherwise check length and angle */
  if (fabs (p0[0] - line->p0[0]) > GCODE_PRECISION ||
      fabs (p0[1] - line->p0[1]) > GCODE_PRECISION ||
      fabs (p1[0] - line->p1[0]) > GCODE_PRECISION ||
      fabs (p1[1] - line->p1[1]) > GCODE_PRECISION)
  {
    gfloat_t dist;

    /* Check the distance between new points - only proceed if length is non zero */
    GCODE_MATH_VEC2D_DIST (dist, p0, p1);

    if (dist < GCODE_PRECISION)
    {
      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[2], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[2]), line->p0[0]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[2], wlist);

      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[3], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[3]), line->p0[1]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[3], wlist);

      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[4], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[4]), line->p1[0]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[4], wlist);

      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[5], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[5]), line->p1[1]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[5], wlist);
      return;
    }

    line->p0[0] = p0[0];
    line->p0[1] = p0[1];
    line->p1[0] = p1[0];
    line->p1[1] = p1[1];

    clength = GCODE_MATH_2D_DISTANCE (line->p1, line->p0);
    gcode_math_xy_to_angle (line->p0, line->p1, &cangle);

    SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[6], wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[6]), clength);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[6], wlist);

    SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[7], wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[7]), cangle);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[7], wlist);
  }
  else
  {
    clength = GCODE_MATH_2D_DISTANCE (line->p1, line->p0);
    gcode_math_xy_to_angle (line->p0, line->p1, &cangle);

    nlength = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));
    nangle = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[7]));

    if (fabs (clength - nlength) > GCODE_PRECISION)
    {
      GCODE_MATH_VEC2D_SUB (vec, line->p1, line->p0);
      GCODE_MATH_VEC2D_UNITIZE (vec);
      GCODE_MATH_VEC2D_MUL_SCALAR (vec, vec, nlength);
      line->p1[0] = line->p0[0] + vec[0];
      line->p1[1] = line->p0[1] + vec[1];

      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[4], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[4]), line->p1[0]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[4], wlist);

      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[5], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[5]), line->p1[1]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[5], wlist);
    }

    if (fabs (cangle - nangle) > GCODE_PRECISION)
    {
      line->p1[0] = line->p0[0] + clength * cos (nangle * GCODE_DEG2RAD);
      line->p1[1] = line->p0[1] + clength * sin (nangle * GCODE_DEG2RAD);

      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[4], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[4]), line->p1[0]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[4], wlist);

      SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[5], wlist);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[5]), line->p1[1]);
      SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[5], wlist);
    }
  }

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_line (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *line_tab;
  GtkWidget *alignment;
  GtkWidget *line_table;
  GtkWidget *label;
  GtkWidget *p0x_spin;
  GtkWidget *p0y_spin;
  GtkWidget *p1x_spin;
  GtkWidget *p1y_spin;
  GtkWidget *length_spin;
  GtkWidget *angle_spin;
  gcode_t *gcode;
  gcode_line_t *line;
  uint16_t row;
  gfloat_t length, angle;

  /**
   * Line Parameters
   */

  gcode = (gcode_t *)block->gcode;

  line = (gcode_line_t *)block->pdata;

  wlist = (GtkWidget **)malloc (8 * sizeof (GtkWidget *));
  row = 0;

  line_tab = gtk_frame_new ("Line Parameters");
  g_signal_connect (line_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), line_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (line_tab), alignment);

  line_table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (line_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (line_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (line_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), line_table);

  if (block->parent->type == GCODE_TYPE_EXTRUSION)
  {
    label = gtk_label_new ("Start Position (X)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p0x_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p0x_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p0x_spin), line->p0[0]);
    g_signal_connect (p0x_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p0x_spin, 1, 2, row, row + 1);
    row++;

    label = gtk_label_new ("Start Position (Y)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p0y_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Z), SCALED_INCHES (MAX_DIM_Z), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p0y_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p0y_spin), line->p0[1]);
    g_signal_connect (p0y_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p0y_spin, 1, 2, row, row + 1);
    row++;

    label = gtk_label_new ("End Position (X)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p1x_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p1x_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p1x_spin), line->p1[0]);
    g_signal_connect (p1x_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p1x_spin, 1, 2, row, row + 1);
    row++;

    label = gtk_label_new ("End Position (Y)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p1y_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Z), SCALED_INCHES (MAX_DIM_Z), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p1y_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p1y_spin), line->p1[1]);
    g_signal_connect (p1y_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p1y_spin, 1, 2, row, row + 1);
    row++;

    length = GCODE_MATH_2D_DISTANCE (line->p1, line->p0);

    label = gtk_label_new ("Length");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    length_spin = gtk_spin_button_new_with_range (GCODE_PRECISION, SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (length_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (length_spin), length);
    g_signal_connect (length_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), length_spin, 1, 2, row, row + 1);
    row++;

    gcode_math_xy_to_angle (line->p0, line->p1, &angle);

    label = gtk_label_new ("Angle");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    angle_spin = gtk_spin_button_new_with_range (0.0, 360.0, 1.0);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (angle_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (angle_spin), angle);
    g_signal_connect (angle_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), angle_spin, 1, 2, row, row + 1);
    row++;
  }
  else
  {
    label = gtk_label_new ("Start Position (X)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p0x_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p0x_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p0x_spin), line->p0[0]);
    g_signal_connect (p0x_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p0x_spin, 1, 2, row, row + 1);
    row++;

    label = gtk_label_new ("Start Position (Y)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p0y_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p0y_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p0y_spin), line->p0[1]);
    g_signal_connect (p0y_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p0y_spin, 1, 2, row, row + 1);
    row++;

    label = gtk_label_new ("End Position (X)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p1x_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p1x_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p1x_spin), line->p1[0]);
    g_signal_connect (p1x_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p1x_spin, 1, 2, row, row + 1);
    row++;

    label = gtk_label_new ("End Position (Y)");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    p1y_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (p1y_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (p1y_spin), line->p1[1]);
    g_signal_connect (p1y_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), p1y_spin, 1, 2, row, row + 1);
    row++;

    length = GCODE_MATH_2D_DISTANCE (line->p1, line->p0);

    label = gtk_label_new ("Length");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    length_spin = gtk_spin_button_new_with_range (GCODE_PRECISION, SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (length_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (length_spin), length);
    g_signal_connect (length_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), length_spin, 1, 2, row, row + 1);
    row++;

    gcode_math_xy_to_angle (line->p0, line->p1, &angle);

    label = gtk_label_new ("Angle");
    gtk_table_attach_defaults (GTK_TABLE (line_table), label, 0, 1, row, row + 1);

    angle_spin = gtk_spin_button_new_with_range (0.0, 360.0, 1.0);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (angle_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (angle_spin), angle);
    g_signal_connect (angle_spin, "value-changed", G_CALLBACK (line_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (line_table), angle_spin, 1, 2, row, row + 1);
    row++;
  }

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = p0x_spin;
  wlist[3] = p0y_spin;
  wlist[4] = p1x_spin;
  wlist[5] = p1y_spin;
  wlist[6] = length_spin;
  wlist[7] = angle_spin;
}

static void
arc_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_arc_t *arc;
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  arc = (gcode_arc_t *)block->pdata;

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));

  if (strcmp (text_field, "Sweep") == 0)
  {
    arc->p[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));
    arc->p[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[7]));
    arc->radius = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[8]));
    arc->start_angle = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[9]));
    arc->sweep_angle = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[10]));

    GCODE_MATH_WRAP_TO_360_DEGREES (arc->start_angle);
  }
  else if (strcmp (text_field, "Radius") == 0)
  {
    gcode_arcdata_t arcdata;
    char *direction, *magnitude;

    direction = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[11]));
    magnitude = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[12]));

    arcdata.fls = strcmp (direction, "CW") ? 1 : 0;
    arcdata.fla = strcmp (magnitude, "Small") ? 1 : 0;

    g_free (direction);
    g_free (magnitude);

    arcdata.p0[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[13]));
    arcdata.p0[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[14]));
    arcdata.p1[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[15]));
    arcdata.p1[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[16]));
    arcdata.radius = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[17]));

    if (gcode_arc_radius_to_sweep (&arcdata) == 0)
    {
      arc->p[0] = arcdata.p0[0];
      arc->p[1] = arcdata.p0[1];
      arc->radius = arcdata.radius;
      arc->start_angle = arcdata.start_angle;
      arc->sweep_angle = arcdata.sweep_angle;
    }
  }
  else if (strcmp (text_field, "Center") == 0)
  {
    gcode_arcdata_t arcdata;
    char *direction;

    direction = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[18]));

    arcdata.fls = strcmp (direction, "CW") ? 1 : 0;

    g_free (direction);

    arcdata.p0[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[19]));
    arcdata.p0[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[20]));
    arcdata.p1[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[21]));
    arcdata.p1[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[22]));
    arcdata.cp[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[23]));
    arcdata.cp[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[24]));

    if (gcode_arc_center_to_sweep (&arcdata) == 0)
    {
      arc->p[0] = arcdata.p0[0];
      arc->p[1] = arcdata.p0[1];
      arc->radius = arcdata.radius;
      arc->start_angle = arcdata.start_angle;
      arc->sweep_angle = arcdata.sweep_angle;
    }
  }

  g_free (text_field);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
arc_interface_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_arc_t *arc;
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  arc = (gcode_arc_t *)block->pdata;

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));

  if (strcmp (text_field, "Sweep") == 0)
  {
    arc->native_mode = GCODE_ARC_INTERFACE_SWEEP;
    gtk_widget_set_child_visible (wlist[3], 1);
    gtk_widget_set_child_visible (wlist[4], 0);
    gtk_widget_set_child_visible (wlist[5], 0);

    SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[6], wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[6]), arc->p[0]);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[6], wlist);

    SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[7], wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[7]), arc->p[1]);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[7], wlist);

    SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[8], wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[8]), arc->radius);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[8], wlist);

    SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[9], wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[9]), arc->start_angle);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[9], wlist);

    SIGNAL_HANDLER_BLOCK_USING_DATA (wlist[10], wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[10]), arc->sweep_angle);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (wlist[10], wlist);
  }
  else if (strcmp (text_field, "Radius") == 0)
  {
    gcode_vec2d_t p0, p1;

    arc->native_mode = GCODE_ARC_INTERFACE_RADIUS;
    gtk_widget_set_child_visible (wlist[3], 0);
    gtk_widget_set_child_visible (wlist[4], 1);
    gtk_widget_set_child_visible (wlist[5], 0);

    gcode_arc_ends (block, p0, p1, GCODE_GET);

    gtk_combo_box_set_active (GTK_COMBO_BOX (wlist[11]), arc->sweep_angle > 0.0 ? 1 : 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (wlist[12]), fabs (arc->sweep_angle) > 180.0 ? 1 : 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[13]), p0[0]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[14]), p0[1]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[15]), p1[0]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[16]), p1[1]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[17]), arc->radius);
  }
  else if (strcmp (text_field, "Center") == 0)
  {
    gcode_vec2d_t p0, p1, center;

    arc->native_mode = GCODE_ARC_INTERFACE_CENTER;
    gtk_widget_set_child_visible (wlist[3], 0);
    gtk_widget_set_child_visible (wlist[4], 0);
    gtk_widget_set_child_visible (wlist[5], 1);

    gcode_arc_ends (block, p0, p1, GCODE_GET);
    gcode_arc_center (block, center, GCODE_GET);

    gtk_combo_box_set_active (GTK_COMBO_BOX (wlist[18]), arc->sweep_angle > 0.0 ? 1 : 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[19]), p0[0]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[20]), p0[1]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[21]), p1[0]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[22]), p1[1]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[23]), center[0]);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[24]), center[1]);
  }

  g_free (text_field);
}

static void
gui_tab_arc (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *arc_tab;
  GtkWidget *alignment;
  GtkWidget *arc_table;
  GtkWidget *arc_sweep_table;
  GtkWidget *arc_radius_table;
  GtkWidget *arc_center_table;
  GtkWidget *label;
  GtkWidget *arc_interface_combo;
  GtkWidget *sweep_start_posx_spin;
  GtkWidget *sweep_start_posy_spin;
  GtkWidget *sweep_radius_spin;
  GtkWidget *sweep_start_angle_spin;
  GtkWidget *sweep_sweep_angle_spin;
  GtkWidget *radius_direction_combo;
  GtkWidget *radius_magnitude_combo;
  GtkWidget *radius_start_posx_spin;
  GtkWidget *radius_start_posy_spin;
  GtkWidget *radius_end_posx_spin;
  GtkWidget *radius_end_posy_spin;
  GtkWidget *radius_radius_spin;
  GtkWidget *center_direction_combo;
  GtkWidget *center_start_posx_spin;
  GtkWidget *center_start_posy_spin;
  GtkWidget *center_end_posx_spin;
  GtkWidget *center_end_posy_spin;
  GtkWidget *center_center_posx_spin;
  GtkWidget *center_center_posy_spin;
  GtkWidget *radius_apply_changes_button;
  GtkWidget *center_apply_changes_button;
  gcode_t *gcode;
  gcode_arc_t *arc;
  gcode_vec2d_t p0, p1, center;
  uint16_t row;

  /**
   * Arc Parameters
   */

  gcode = (gcode_t *)block->gcode;

  arc = (gcode_arc_t *)block->pdata;

  gcode_arc_ends (block, p0, p1, GCODE_GET);
  gcode_arc_center (block, center, GCODE_GET);

  wlist = (GtkWidget **)malloc (25 * sizeof (GtkWidget *));
  row = 0;

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;

  arc_tab = gtk_frame_new ("Arc Parameters");
  g_signal_connect (arc_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), arc_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (arc_tab), alignment);

  arc_table = gtk_table_new (8, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (arc_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (arc_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (arc_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), arc_table);

  label = gtk_label_new ("Arc Interface");
  gtk_table_attach_defaults (GTK_TABLE (arc_table), label, 0, 1, row, row + 1);

  arc_interface_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (arc_interface_combo), "Sweep");
  gtk_combo_box_append_text (GTK_COMBO_BOX (arc_interface_combo), "Radius");
  gtk_combo_box_append_text (GTK_COMBO_BOX (arc_interface_combo), "Center");
  gtk_combo_box_set_active (GTK_COMBO_BOX (arc_interface_combo), arc->native_mode);
  g_signal_connect (arc_interface_combo, "changed", G_CALLBACK (arc_interface_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (arc_table), arc_interface_combo, 1, 2, row, row + 1);
  row++;

  wlist[2] = arc_interface_combo;

  arc_sweep_table = gtk_table_new (8, 2, TRUE);
  gtk_table_set_col_spacings (GTK_TABLE (arc_sweep_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (arc_sweep_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (arc_sweep_table), 0);
  gtk_table_attach_defaults (GTK_TABLE (arc_table), arc_sweep_table, 0, 2, row, row + 1);

  arc_radius_table = gtk_table_new (8, 2, TRUE);
  gtk_table_set_col_spacings (GTK_TABLE (arc_radius_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (arc_radius_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (arc_radius_table), 0);
  gtk_table_attach_defaults (GTK_TABLE (arc_table), arc_radius_table, 0, 2, row, row + 1);

  arc_center_table = gtk_table_new (8, 2, TRUE);
  gtk_table_set_col_spacings (GTK_TABLE (arc_center_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (arc_center_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (arc_center_table), 0);
  gtk_table_attach_defaults (GTK_TABLE (arc_table), arc_center_table, 0, 2, row, row + 1);

  wlist[3] = arc_sweep_table;
  wlist[4] = arc_radius_table;
  wlist[5] = arc_center_table;

  gtk_widget_set_child_visible (wlist[3], 0);
  gtk_widget_set_child_visible (wlist[4], 0);
  gtk_widget_set_child_visible (wlist[5], 0);
  gtk_widget_set_child_visible (wlist[arc->native_mode + 3], 1);

  /**
   * SWEEP INTERFACE
   */

  row = 0;
  label = gtk_label_new ("Start Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), label, 0, 1, row, row + 1);

  sweep_start_posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sweep_start_posx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sweep_start_posx_spin), p0[0]);
  g_signal_connect (sweep_start_posx_spin, "value-changed", G_CALLBACK (arc_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), sweep_start_posx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Start Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), label, 0, 1, row, row + 1);

  sweep_start_posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sweep_start_posy_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sweep_start_posy_spin), p0[1]);
  g_signal_connect (sweep_start_posy_spin, "value-changed", G_CALLBACK (arc_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), sweep_start_posy_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Radius");
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), label, 0, 1, row, row + 1);

  sweep_radius_spin = gtk_spin_button_new_with_range (GCODE_PRECISION, SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sweep_radius_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sweep_radius_spin), arc->radius);
  g_signal_connect (sweep_radius_spin, "value-changed", G_CALLBACK (arc_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), sweep_radius_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Start Angle");
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), label, 0, 1, row, row + 1);

  sweep_start_angle_spin = gtk_spin_button_new_with_range (0.0, 360.0, 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sweep_start_angle_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sweep_start_angle_spin), arc->start_angle);
  g_signal_connect (sweep_start_angle_spin, "value-changed", G_CALLBACK (arc_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), sweep_start_angle_spin, 1, 2, row, row + 1);
  row++;

  if (block->parent->type == GCODE_TYPE_EXTRUSION)
  {
    label = gtk_label_new ("Sweep");
    gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), label, 0, 1, row, row + 1);

    sweep_sweep_angle_spin = gtk_spin_button_new_with_range (-90.0, 90.0, 1.0);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sweep_sweep_angle_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (sweep_sweep_angle_spin), arc->sweep_angle);
    g_signal_connect (sweep_sweep_angle_spin, "value-changed", G_CALLBACK (arc_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), sweep_sweep_angle_spin, 1, 2, row, row + 1);
    row++;
  }
  else
  {
    label = gtk_label_new ("Sweep");
    gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), label, 0, 1, row, row + 1);

    sweep_sweep_angle_spin = gtk_spin_button_new_with_range (-360.0, 360.0, 1.0);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sweep_sweep_angle_spin), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (sweep_sweep_angle_spin), arc->sweep_angle);
    g_signal_connect (sweep_sweep_angle_spin, "value-changed", G_CALLBACK (arc_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (arc_sweep_table), sweep_sweep_angle_spin, 1, 2, row, row + 1);
    row++;
  }

  wlist[6] = sweep_start_posx_spin;
  wlist[7] = sweep_start_posy_spin;
  wlist[8] = sweep_radius_spin;
  wlist[9] = sweep_start_angle_spin;
  wlist[10] = sweep_sweep_angle_spin;

  /**
   * RADIUS INTERFACE
   */

  row = 0;
  label = gtk_label_new ("Direction");
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), label, 0, 1, row, row + 1);

  radius_direction_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (radius_direction_combo), "CW");
  gtk_combo_box_append_text (GTK_COMBO_BOX (radius_direction_combo), "CCW");
  gtk_combo_box_set_active (GTK_COMBO_BOX (radius_direction_combo), arc->sweep_angle > 0.0 ? 1 : 0);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_direction_combo, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Magnitude");
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), label, 0, 1, row, row + 1);

  radius_magnitude_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (radius_magnitude_combo), "Small");
  gtk_combo_box_append_text (GTK_COMBO_BOX (radius_magnitude_combo), "Large");
  gtk_combo_box_set_active (GTK_COMBO_BOX (radius_magnitude_combo), fabs (arc->sweep_angle) > 180.0 ? 1 : 0);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_magnitude_combo, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Start Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), label, 0, 1, row, row + 1);

  radius_start_posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (radius_start_posx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (radius_start_posx_spin), p0[0]);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_start_posx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Start Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), label, 0, 1, row, row + 1);

  radius_start_posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (radius_start_posy_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (radius_start_posy_spin), p0[1]);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_start_posy_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("End Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), label, 0, 1, row, row + 1);

  radius_end_posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (radius_end_posx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (radius_end_posx_spin), p1[0]);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_end_posx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("End Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), label, 0, 1, row, row + 1);

  radius_end_posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (radius_end_posy_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (radius_end_posy_spin), p1[1]);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_end_posy_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Radius");
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), label, 0, 1, row, row + 1);

  radius_radius_spin = gtk_spin_button_new_with_range (GCODE_PRECISION, SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (radius_radius_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (radius_radius_spin), arc->radius);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_radius_spin, 1, 2, row, row + 1);
  row++;

  radius_apply_changes_button = gtk_button_new_with_label ("Apply Changes");
  g_signal_connect (radius_apply_changes_button, "clicked", G_CALLBACK (arc_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (arc_radius_table), radius_apply_changes_button, 0, 2, row, row + 1);

  wlist[11] = radius_direction_combo;
  wlist[12] = radius_magnitude_combo;
  wlist[13] = radius_start_posx_spin;
  wlist[14] = radius_start_posy_spin;
  wlist[15] = radius_end_posx_spin;
  wlist[16] = radius_end_posy_spin;
  wlist[17] = radius_radius_spin;

  /**
   * CENTER INTERFACE
   */

  row = 0;
  label = gtk_label_new ("Direction");
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), label, 0, 1, row, row + 1);

  center_direction_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (center_direction_combo), "CW");
  gtk_combo_box_append_text (GTK_COMBO_BOX (center_direction_combo), "CCW");
  gtk_combo_box_set_active (GTK_COMBO_BOX (center_direction_combo), arc->sweep_angle > 0 ? 1 : 0);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_direction_combo, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Start Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), label, 0, 1, row, row + 1);

  center_start_posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (center_start_posx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (center_start_posx_spin), p0[0]);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_start_posx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Start Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), label, 0, 1, row, row + 1);

  center_start_posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (center_start_posy_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (center_start_posy_spin), p0[1]);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_start_posy_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("End Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), label, 0, 1, row, row + 1);

  center_end_posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (center_end_posx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (center_end_posx_spin), p1[0]);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_end_posx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("End Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), label, 0, 1, row, row + 1);

  center_end_posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (center_end_posy_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (center_end_posy_spin), p1[1]);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_end_posy_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Center Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), label, 0, 1, row, row + 1);

  center_center_posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (center_center_posx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (center_center_posx_spin), center[0]);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_center_posx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Center Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), label, 0, 1, row, row + 1);

  center_center_posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (center_center_posy_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (center_center_posy_spin), center[1]);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_center_posy_spin, 1, 2, row, row + 1);
  row++;

  center_apply_changes_button = gtk_button_new_with_label ("Apply Changes");
  g_signal_connect (center_apply_changes_button, "clicked", G_CALLBACK (arc_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (arc_center_table), center_apply_changes_button, 0, 2, row, row + 1);

  wlist[18] = center_direction_combo;
  wlist[19] = center_start_posx_spin;
  wlist[20] = center_start_posy_spin;
  wlist[21] = center_end_posx_spin;
  wlist[22] = center_end_posy_spin;
  wlist[23] = center_center_posx_spin;
  wlist[24] = center_center_posy_spin;
}

static void
bolt_holes_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_bolt_holes_t *bolt_holes;
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  bolt_holes->position[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  bolt_holes->position[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  bolt_holes->hole_diameter = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[4]));
  bolt_holes->offset_distance = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[6]));

  if ((strcmp (text_field, "Radial") == 0) && (bolt_holes->type != GCODE_BOLT_HOLES_TYPE_RADIAL))
  {
    bolt_holes->type = GCODE_BOLT_HOLES_TYPE_RADIAL;

    /* Update the labels and spin buttons */
    gtk_label_set_text (GTK_LABEL (wlist[7]), "Hole Num");
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (wlist[8]), 1, 100);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (wlist[8]), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[8]), bolt_holes->number[0]);

    gtk_label_set_text (GTK_LABEL (wlist[9]), "Offset Angle");
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (wlist[10]), 0.0, 360.0);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (wlist[10]), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[10]), bolt_holes->offset_angle);
  }
  else if ((strcmp (text_field, "Matrix") == 0) && (bolt_holes->type != GCODE_BOLT_HOLES_TYPE_MATRIX))
  {
    bolt_holes->type = GCODE_BOLT_HOLES_TYPE_MATRIX;

    /* Update the labels and spin buttons */
    gtk_label_set_text (GTK_LABEL (wlist[7]), "Hole Number (X)");
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (wlist[8]), 1, 100);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (wlist[8]), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[8]), bolt_holes->number[0]);

    gtk_label_set_text (GTK_LABEL (wlist[9]), "Hole Number (Y)");
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (wlist[10]), 1, 100);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (wlist[10]), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[10]), bolt_holes->number[1]);
  }

  bolt_holes->number[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[8]));

  if (strcmp (text_field, "Radial") == 0)
  {
    bolt_holes->type = GCODE_BOLT_HOLES_TYPE_RADIAL;
    bolt_holes->offset_angle = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[10]));

    GCODE_MATH_WRAP_TO_360_DEGREES (bolt_holes->offset_angle);
  }
  else if (strcmp (text_field, "Matrix") == 0)
  {
    bolt_holes->type = GCODE_BOLT_HOLES_TYPE_MATRIX;
    bolt_holes->number[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[10]));
  }

  g_free (text_field);

  bolt_holes->pocket = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[11]));

  gcode_bolt_holes_rebuild (block);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_bolt_holes (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *bolt_holes_tab;
  GtkWidget *alignment;
  GtkWidget *bolt_holes_table;
  GtkWidget *label;
  GtkWidget *posx_spin;
  GtkWidget *posy_spin;
  GtkWidget *type_combo;
  GtkWidget *generic_spin[2];
  GtkWidget *hole_diameter_spin;
  GtkWidget *offset_distance_spin;
  GtkWidget *pocket_check_button;
  gcode_t *gcode;
  gcode_bolt_holes_t *bolt_holes;

  /**
   * Bolt Holes Parameters
   */

  gcode = (gcode_t *)block->gcode;

  bolt_holes = (gcode_bolt_holes_t *)block->pdata;

  wlist = (GtkWidget **)malloc (12 * sizeof (GtkWidget *));

  bolt_holes_tab = gtk_frame_new ("Bolt Holes Parameters");
  g_signal_connect (bolt_holes_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), bolt_holes_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (bolt_holes_tab), alignment);

  bolt_holes_table = gtk_table_new (8, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (bolt_holes_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (bolt_holes_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (bolt_holes_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), bolt_holes_table);

  label = gtk_label_new ("Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 0, 1);

  posx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (posx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (posx_spin), bolt_holes->position[0]);
  g_signal_connect (posx_spin, "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), posx_spin, 1, 2, 0, 1);

  label = gtk_label_new ("Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 1, 2);

  posy_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (posy_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (posy_spin), bolt_holes->position[1]);
  g_signal_connect (posy_spin, "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), posy_spin, 1, 2, 1, 2);

  label = gtk_label_new ("Hole Diameter");
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 2, 3);

  hole_diameter_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (hole_diameter_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (hole_diameter_spin), bolt_holes->hole_diameter);
  g_signal_connect (hole_diameter_spin, "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), hole_diameter_spin, 1, 2, 2, 3);

  label = gtk_label_new ("Offset Distance");
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 3, 4);

  offset_distance_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (offset_distance_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (offset_distance_spin), bolt_holes->offset_distance);
  g_signal_connect (offset_distance_spin, "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), offset_distance_spin, 1, 2, 3, 4);

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = posx_spin;
  wlist[3] = posy_spin;
  wlist[4] = hole_diameter_spin;
  wlist[5] = offset_distance_spin;

  label = gtk_label_new ("Type");
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 4, 5);

  type_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (type_combo), "Radial");
  gtk_combo_box_append_text (GTK_COMBO_BOX (type_combo), "Matrix");
  gtk_combo_box_set_active (GTK_COMBO_BOX (type_combo), bolt_holes->type);
  g_signal_connect (type_combo, "changed", G_CALLBACK (bolt_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), type_combo, 1, 2, 4, 5);
  wlist[6] = type_combo;

  if (bolt_holes->type == GCODE_BOLT_HOLES_TYPE_RADIAL)
  {
    label = gtk_label_new ("Hole Num");
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 5, 6);

    generic_spin[0] = gtk_spin_button_new_with_range (1, 100, 1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (generic_spin[0]), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (generic_spin[0]), bolt_holes->number[0]);
    g_signal_connect (generic_spin[0], "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), generic_spin[0], 1, 2, 5, 6);
    wlist[7] = label;
    wlist[8] = generic_spin[0];

    label = gtk_label_new ("Offset Angle");
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 6, 7);

    generic_spin[1] = gtk_spin_button_new_with_range (0.0, 360.0, 1.0);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (generic_spin[1]), MANTISSA);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (generic_spin[1]), bolt_holes->offset_angle);
    g_signal_connect (generic_spin[1], "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), generic_spin[1], 1, 2, 6, 7);
    wlist[9] = label;
    wlist[10] = generic_spin[1];
  }
  else if (bolt_holes->type == GCODE_BOLT_HOLES_TYPE_MATRIX)
  {
    label = gtk_label_new ("Hole Number (X)");
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 5, 6);

    generic_spin[0] = gtk_spin_button_new_with_range (1, 100, 1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (generic_spin[0]), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (generic_spin[0]), bolt_holes->number[0]);
    g_signal_connect (generic_spin[0], "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), generic_spin[0], 1, 2, 5, 6);
    wlist[7] = label;
    wlist[8] = generic_spin[0];

    label = gtk_label_new ("Hole Number (Y)");
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 6, 7);

    generic_spin[1] = gtk_spin_button_new_with_range (1, 100, 1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (generic_spin[1]), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (generic_spin[1]), bolt_holes->number[1]);
    g_signal_connect (generic_spin[1], "value-changed", G_CALLBACK (bolt_holes_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), generic_spin[1], 1, 2, 6, 7);
    wlist[9] = label;
    wlist[10] = generic_spin[1];
  }

  label = gtk_label_new ("Pocket");
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), label, 0, 1, 7, 8);

  pocket_check_button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pocket_check_button), bolt_holes->pocket);
  g_signal_connect (pocket_check_button, "toggled", G_CALLBACK (bolt_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (bolt_holes_table), pocket_check_button, 1, 2, 7, 8);
  wlist[11] = pocket_check_button;
}

static void
drill_holes_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_drill_holes_t *drill_holes;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  drill_holes = (gcode_drill_holes_t *)block->pdata;

  drill_holes->depth = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  drill_holes->increment = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  drill_holes->optimal_path = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[4]));

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_drill_holes (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *drill_holes_tab;
  GtkWidget *alignment;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *depth_spin;
  GtkWidget *peck_increment_spin;
  GtkWidget *optimal_path_check_button;
  gcode_t *gcode;
  gcode_drill_holes_t *drill_holes;

  /**
   * Drill Holes Parameters
   */

  gcode = (gcode_t *)block->gcode;

  drill_holes = (gcode_drill_holes_t *)block->pdata;

  wlist = (GtkWidget **)malloc (5 * sizeof (GtkWidget *));

  drill_holes_tab = gtk_frame_new ("Drill Holes Parameters");
  g_signal_connect (drill_holes_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), drill_holes_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (drill_holes_tab), alignment);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  label = gtk_label_new ("Depth");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

  depth_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Z), SCALED_INCHES (0.0), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (depth_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (depth_spin), drill_holes->depth);
  g_signal_connect (depth_spin, "value-changed", G_CALLBACK (drill_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), depth_spin, 1, 2, 0, 1);

  label = gtk_label_new ("Peck Increment");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

  peck_increment_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (MAX_DIM_Z), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (peck_increment_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (peck_increment_spin), drill_holes->increment);
  g_signal_connect (peck_increment_spin, "value-changed", G_CALLBACK (drill_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), peck_increment_spin, 1, 2, 1, 2);

  label = gtk_label_new ("Optimal Path");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);

  optimal_path_check_button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (optimal_path_check_button), drill_holes->optimal_path);
  g_signal_connect (optimal_path_check_button, "toggled", G_CALLBACK (drill_holes_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), optimal_path_check_button, 1, 2, 2, 3);

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = (GtkWidget *)depth_spin;
  wlist[3] = (GtkWidget *)peck_increment_spin;
  wlist[4] = (GtkWidget *)optimal_path_check_button;
}

static void
point_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_point_t *point;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  point = (gcode_point_t *)block->pdata;

  point->p[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  point->p[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_point (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *point_tab;
  GtkWidget *alignment;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *px_spin;
  GtkWidget *py_spin;
  gcode_t *gcode;
  gcode_point_t *point;
  uint16_t row;

  /**
   * Point Parameters
   */

  gcode = (gcode_t *)block->gcode;

  point = (gcode_point_t *)block->pdata;

  wlist = (GtkWidget **)malloc (4 * sizeof (GtkWidget *));

  row = 0;

  point_tab = gtk_frame_new ("Point Parameters");
  g_signal_connect (point_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), point_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (point_tab), alignment);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  label = gtk_label_new ("Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, row, row + 1);

  px_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (px_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (px_spin), point->p[0]);
  g_signal_connect (px_spin, "value-changed", G_CALLBACK (point_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), px_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, row, row + 1);

  py_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (py_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (py_spin), point->p[1]);
  g_signal_connect (py_spin, "value-changed", G_CALLBACK (point_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), py_spin, 1, 2, row, row + 1);
  row++;

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = px_spin;
  wlist[3] = py_spin;
}

static void
template_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_template_t *template;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  template = (gcode_template_t *)block->pdata;

  template->position[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  template->position[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  template->rotation = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[4]));

  GCODE_MATH_WRAP_TO_360_DEGREES (template->rotation);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_template (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *template_tab;
  GtkWidget *alignment;
  GtkWidget *template_table;
  GtkWidget *label;
  GtkWidget *positionx_spin;
  GtkWidget *positiony_spin;
  GtkWidget *rotation_spin;
  gcode_t *gcode;
  gcode_template_t *template;
  uint16_t row;

  /**
   * Template Parameters
   */

  gcode = (gcode_t *)block->gcode;

  template = (gcode_template_t *)block->pdata;

  wlist = (GtkWidget **)malloc (5 * sizeof (GtkWidget *));

  row = 0;

  template_tab = gtk_frame_new ("Template Parameters");
  g_signal_connect (template_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), template_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (template_tab), alignment);

  template_table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (template_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (template_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (template_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), template_table);

  label = gtk_label_new ("Position (X)");
  gtk_table_attach_defaults (GTK_TABLE (template_table), label, 0, 1, row, row + 1);

  positionx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_X), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (positionx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (positionx_spin), template->position[0]);
  g_signal_connect (positionx_spin, "value-changed", G_CALLBACK (template_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (template_table), positionx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Position (Y)");
  gtk_table_attach_defaults (GTK_TABLE (template_table), label, 0, 1, row, row + 1);

  positiony_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Y), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (positiony_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (positiony_spin), template->position[1]);
  g_signal_connect (positiony_spin, "value-changed", G_CALLBACK (template_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (template_table), positiony_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Rotation");
  gtk_table_attach_defaults (GTK_TABLE (template_table), label, 0, 1, row, row + 1);

  rotation_spin = gtk_spin_button_new_with_range (0.0, 360.0, 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rotation_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (rotation_spin), template->rotation);
  g_signal_connect (rotation_spin, "value-changed", G_CALLBACK (template_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (template_table), rotation_spin, 1, 2, row, row + 1);
  row++;

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = positionx_spin;
  wlist[3] = positiony_spin;
  wlist[4] = rotation_spin;
}

static void
sketch_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_sketch_t *sketch;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  sketch = (gcode_sketch_t *)block->pdata;

  sketch->taper_offset[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  sketch->taper_offset[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  sketch->zero_pass = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[5]));

  if (fabs (sketch->taper_offset[0]) > 0.0 || fabs (sketch->taper_offset[1]) > 0.0)
  {
    gtk_widget_set_sensitive (wlist[6], 0);
    sketch->helical = 0;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wlist[6]), sketch->helical);
  }
  else
  {
    gtk_widget_set_sensitive (wlist[6], 1);
  }

  if (sketch->pocket && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[6])))
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wlist[4]), 0);
    sketch->pocket = 0;
    sketch->helical = 1;
  }
  else if (sketch->helical && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[4])))
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wlist[6]), 0);
    sketch->helical = 0;
    sketch->pocket = 1;
  }
  else
  {
    sketch->pocket = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[4]));
    sketch->helical = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[6]));
  }

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_sketch (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *sketch_tab;
  GtkWidget *alignment;
  GtkWidget *sketch_table;
  GtkWidget *label;
  GtkWidget *taper_offsetx_spin;
  GtkWidget *taper_offsety_spin;
  GtkWidget *pocket_check_button;
  GtkWidget *zero_pass_check_button;
  GtkWidget *helical_check_button;
  gcode_t *gcode;
  gcode_sketch_t *sketch;
  int taper_exists;
  uint16_t row;

  /**
   * Sketch Parameters
   */

  gcode = (gcode_t *)block->gcode;

  sketch = (gcode_sketch_t *)block->pdata;

  wlist = (GtkWidget **)malloc (7 * sizeof (GtkWidget *));

  row = 0;

  sketch_tab = gtk_frame_new ("Sketch Parameters");
  g_signal_connect (sketch_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), sketch_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (sketch_tab), alignment);

  sketch_table = gtk_table_new (4, 4, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (sketch_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (sketch_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (sketch_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), sketch_table);

  label = gtk_label_new ("Taper Offset (X)");
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), label, 0, 1, row, row + 1);

  taper_offsetx_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-1.0), SCALED_INCHES (1.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (taper_offsetx_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (taper_offsetx_spin), sketch->taper_offset[0]);
  g_signal_connect (taper_offsetx_spin, "value-changed", G_CALLBACK (sketch_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), taper_offsetx_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Taper Offset (Y)");
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), label, 0, 1, row, row + 1);

  taper_offsety_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-1.0), SCALED_INCHES (1.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (taper_offsety_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (taper_offsety_spin), sketch->taper_offset[1]);
  g_signal_connect (taper_offsety_spin, "value-changed", G_CALLBACK (sketch_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), taper_offsety_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Pocket");
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), label, 0, 1, row, row + 1);

  pocket_check_button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pocket_check_button), sketch->pocket);
  g_signal_connect (pocket_check_button, "toggled", G_CALLBACK (sketch_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), pocket_check_button, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Zero Pass");
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), label, 0, 1, row, row + 1);

  zero_pass_check_button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (zero_pass_check_button), sketch->zero_pass);
  g_signal_connect (zero_pass_check_button, "toggled", G_CALLBACK (sketch_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), zero_pass_check_button, 1, 2, row, row + 1);
  row++;

  taper_exists = gcode_extrusion_taper_exists (block->extruder);

  if (taper_exists || (fabs (sketch->taper_offset[0]) > GCODE_PRECISION) || (fabs (sketch->taper_offset[1]) > GCODE_PRECISION))
    sketch->helical = 0;

  label = gtk_label_new ("Helical");
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), label, 0, 1, row, row + 1);

  helical_check_button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (helical_check_button), sketch->helical);
  g_signal_connect (helical_check_button, "toggled", G_CALLBACK (sketch_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (sketch_table), helical_check_button, 1, 2, row, row + 1);
  row++;

  if (taper_exists || (fabs (sketch->taper_offset[0]) > GCODE_PRECISION) || (fabs (sketch->taper_offset[1]) > GCODE_PRECISION))
    gtk_widget_set_sensitive (helical_check_button, 0);

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = taper_offsetx_spin;
  wlist[3] = taper_offsety_spin;
  wlist[4] = pocket_check_button;
  wlist[5] = zero_pass_check_button;
  wlist[6] = helical_check_button;
}

static void
extrusion_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_extrusion_t *extrusion;
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  extrusion = (gcode_extrusion_t *)block->pdata;

  extrusion->resolution = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[3]));

  if (strcmp (text_field, "Inside") == 0)
  {
    extrusion->cut_side = GCODE_EXTRUSION_INSIDE;
  }
  else if (strcmp (text_field, "Outside") == 0)
  {
    extrusion->cut_side = GCODE_EXTRUSION_OUTSIDE;
  }
  else if (strcmp (text_field, "Along") == 0)
  {
    extrusion->cut_side = GCODE_EXTRUSION_ALONG;
  }

  g_free (text_field);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_extrusion (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *extrusion_tab;
  GtkWidget *alignment;
  GtkWidget *extrusion_table;
  GtkWidget *label;
  GtkWidget *resolution_spin;
  GtkWidget *cut_side_combo;
  gcode_t *gcode;
  gcode_extrusion_t *extrusion;
  uint16_t row;
  uint8_t option = 1;

  /**
   * Extrusion Parameters
   */

  gcode = (gcode_t *)block->gcode;

  extrusion = (gcode_extrusion_t *)block->pdata;

  wlist = (GtkWidget **)malloc (4 * sizeof (GtkWidget *));

  row = 0;

  extrusion_tab = gtk_frame_new ("Extrusion Parameters");
  g_signal_connect (extrusion_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), extrusion_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (extrusion_tab), alignment);

  extrusion_table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (extrusion_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (extrusion_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (extrusion_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), extrusion_table);

  label = gtk_label_new ("Resolution");
  gtk_table_attach_defaults (GTK_TABLE (extrusion_table), label, 0, 1, row, row + 1);

  resolution_spin = gtk_spin_button_new_with_range (0.001, gcode->material_size[2], SCALED_INCHES (0.001));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (resolution_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (resolution_spin), extrusion->resolution);
  g_signal_connect (resolution_spin, "value-changed", G_CALLBACK (extrusion_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (extrusion_table), resolution_spin, 1, 2, row, row + 1);
  row++;

  if (block->parent->type == GCODE_TYPE_SKETCH)
    option = gcode_sketch_is_closed (block->parent);

  label = gtk_label_new ("Cut Side");
  gtk_table_attach_defaults (GTK_TABLE (extrusion_table), label, 0, 1, row, row + 1);

  cut_side_combo = gtk_combo_box_new_text ();

  if (option)
  {
    gtk_combo_box_append_text (GTK_COMBO_BOX (cut_side_combo), "Inside");
    gtk_combo_box_append_text (GTK_COMBO_BOX (cut_side_combo), "Outside");
    gtk_combo_box_append_text (GTK_COMBO_BOX (cut_side_combo), "Along");
    gtk_combo_box_set_active (GTK_COMBO_BOX (cut_side_combo), extrusion->cut_side);
  }
  else
  {
    extrusion->cut_side = GCODE_EXTRUSION_ALONG;
    gtk_combo_box_append_text (GTK_COMBO_BOX (cut_side_combo), "Along");
    gtk_combo_box_set_active (GTK_COMBO_BOX (cut_side_combo), 0);
  }

  g_signal_connect (cut_side_combo, "changed", G_CALLBACK (extrusion_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (extrusion_table), cut_side_combo, 1, 2, row, row + 1);
  row++;

  if (block->parent->type == GCODE_TYPE_BOLT_HOLES)
  {
    /* Do not allow the user to change this, bolt holes by definition are inside only. */
    gtk_widget_set_sensitive (cut_side_combo, 0);
  }

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = resolution_spin;
  wlist[3] = cut_side_combo;
}

static void
tool_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_tool_t *tool;
  gui_endmill_t *endmill;
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  tool = (gcode_tool_t *)block->pdata;

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));
  strcpy (tool->label, &text_field[6]);
  g_free (text_field);

  endmill = gui_endmills_find (&gui->endmills, tool->label, TRUE);

  tool->diameter = gui_endmills_size (endmill, gui->gcode.units);
  tool->number = endmill->number;

  tool->feed = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[4]));

  if (strcmp (text_field, "On") == 0)
  {
    tool->prompt = 1;
  }
  else if (strcmp (text_field, "Off") == 0)
  {
    tool->prompt = 0;
  }

  g_free (text_field);

  tool->change_position[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));
  tool->change_position[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));
  tool->change_position[2] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[7]));

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[8]));

  if (strcmp (text_field, "100%") == 0)
    tool->plunge_ratio = 1.0;
  else if (strcmp (text_field, "50%") == 0)
    tool->plunge_ratio = 0.5;
  else if (strcmp (text_field, "20%") == 0)
    tool->plunge_ratio = 0.2;
  else if (strcmp (text_field, "10%") == 0)
    tool->plunge_ratio = 0.1;

  g_free (text_field);

  tool->spindle_rpm = (uint32_t)gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[9]));

  tool->coolant = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wlist[10]));

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_tool (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *tool_tab;
  GtkWidget *alignment;
  GtkWidget *tool_table;
  GtkWidget *label;
  GtkWidget *prompt_combo;
  GtkWidget *changex_spin;
  GtkWidget *changey_spin;
  GtkWidget *changez_spin;
  GtkWidget *end_mill_combo;
  GtkWidget *feed_spin;
  GtkWidget *plunge_ratio_combo;
  GtkWidget *spindle_rpm_spin;
  GtkWidget *coolant_check_button;
  gcode_t *gcode;
  gcode_tool_t *tool;
  uint16_t row;
  int selind;

  /**
   * Tool Parameters
   */

  gcode = (gcode_t *)block->gcode;

  tool = (gcode_tool_t *)block->pdata;

  wlist = (GtkWidget **)malloc (11 * sizeof (GtkWidget *));

  row = 0;

  tool_tab = gtk_frame_new ("Tool Parameters");
  g_signal_connect (tool_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), tool_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (tool_tab), alignment);

  tool_table = gtk_table_new (12, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (tool_table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (tool_table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (tool_table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), tool_table);

  {
    char string[32];
    int i;

    label = gtk_label_new ("End Mill");
    gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 2, row, row + 1);
    row++;

    end_mill_combo = gtk_combo_box_new_text ();
    selind = -1;

    for (i = 0; i < gui->endmills.number; i++)
    {
      if (strcmp (tool->label, gui->endmills.endmill[i].description) == 0)
        selind = i;

      sprintf (string, "T%.2d - %s", gui->endmills.endmill[i].number, gui->endmills.endmill[i].description);
      gtk_combo_box_append_text (GTK_COMBO_BOX (end_mill_combo), string);
    }

    if (selind == -1)
    {
      sprintf (string, "#%.2d - %s", tool->number, tool->label);
      gtk_combo_box_append_text (GTK_COMBO_BOX (end_mill_combo), string);
      selind = i;
    }

    gtk_combo_box_set_active (GTK_COMBO_BOX (end_mill_combo), selind);

    g_signal_connect (end_mill_combo, "changed", G_CALLBACK (tool_update_callback), wlist);
    gtk_table_attach_defaults (GTK_TABLE (tool_table), end_mill_combo, 0, 2, row, row + 1);
  }

  row++;

  label = gtk_label_new ("Feed Rate");
  gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);

  feed_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.01), SCALED_INCHES (80.0), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (feed_spin), 2);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (feed_spin), tool->feed);
  g_signal_connect (feed_spin, "value-changed", G_CALLBACK (tool_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (tool_table), feed_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Prompt");
  gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);

  prompt_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (prompt_combo), "Off");
  gtk_combo_box_append_text (GTK_COMBO_BOX (prompt_combo), "On");
  gtk_combo_box_set_active (GTK_COMBO_BOX (prompt_combo), tool->prompt);
  g_signal_connect (prompt_combo, "changed", G_CALLBACK (tool_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (tool_table), prompt_combo, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Change At (X)");

  changex_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (20.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (changex_spin), 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (changex_spin), tool->change_position[0]);
  g_signal_connect (changex_spin, "value-changed", G_CALLBACK (tool_update_callback), wlist);

  if ((gcode->machine_options & GCODE_MACHINE_OPTION_AUTOMATIC_TOOL_CHANGE) == 0)
  {
    gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (tool_table), changex_spin, 1, 2, row, row + 1);
    row++;
  }

  label = gtk_label_new ("Change At (Y)");

  changey_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (20.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (changey_spin), 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (changey_spin), tool->change_position[1]);
  g_signal_connect (changey_spin, "value-changed", G_CALLBACK (tool_update_callback), wlist);

  if ((gcode->machine_options & GCODE_MACHINE_OPTION_AUTOMATIC_TOOL_CHANGE) == 0)
  {
    gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (tool_table), changey_spin, 1, 2, row, row + 1);
    row++;
  }

  label = gtk_label_new ("Change At (Z)");

  changez_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (3.0), SCALED_INCHES (0.1));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (changez_spin), 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (changez_spin), tool->change_position[2]);
  g_signal_connect (changez_spin, "value-changed", G_CALLBACK (tool_update_callback), wlist);

  if ((gcode->machine_options & GCODE_MACHINE_OPTION_AUTOMATIC_TOOL_CHANGE) == 0)
  {
    gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (tool_table), changez_spin, 1, 2, row, row + 1);
    row++;
  }

  label = gtk_label_new ("Plunge Ratio");
  gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);

  plunge_ratio_combo = gtk_combo_box_new_text ();

  gtk_combo_box_append_text (GTK_COMBO_BOX (plunge_ratio_combo), "100%");
  gtk_combo_box_append_text (GTK_COMBO_BOX (plunge_ratio_combo), "50%");
  gtk_combo_box_append_text (GTK_COMBO_BOX (plunge_ratio_combo), "20%");
  gtk_combo_box_append_text (GTK_COMBO_BOX (plunge_ratio_combo), "10%");

  if (tool->plunge_ratio == 1.0)
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (plunge_ratio_combo), 0);
  }
  else if (tool->plunge_ratio == 0.5)
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (plunge_ratio_combo), 1);
  }
  else if (tool->plunge_ratio == 0.2)
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (plunge_ratio_combo), 2);
  }
  else
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (plunge_ratio_combo), 3);
  }

  g_signal_connect (plunge_ratio_combo, "changed", G_CALLBACK (tool_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (tool_table), plunge_ratio_combo, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Spindle RPM");

  spindle_rpm_spin = gtk_spin_button_new_with_range (15.0, 80000.0, 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spindle_rpm_spin), 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spindle_rpm_spin), tool->spindle_rpm);
  g_signal_connect (spindle_rpm_spin, "value-changed", G_CALLBACK (tool_update_callback), wlist);

  if (gcode->machine_options & GCODE_MACHINE_OPTION_SPINDLE_CONTROL)
  {
    gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (tool_table), spindle_rpm_spin, 1, 2, row, row + 1);
    row++;
  }

  label = gtk_label_new ("Coolant");

  coolant_check_button = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (coolant_check_button), tool->coolant);
  g_signal_connect (coolant_check_button, "toggled", G_CALLBACK (tool_update_callback), wlist);

  if (gcode->machine_options & GCODE_MACHINE_OPTION_COOLANT)
  {
    gtk_table_attach_defaults (GTK_TABLE (tool_table), label, 0, 1, row, row + 1);
    gtk_table_attach_defaults (GTK_TABLE (tool_table), coolant_check_button, 1, 2, row, row + 1);
    row++;
  }

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = end_mill_combo;
  wlist[3] = feed_spin;
  wlist[4] = prompt_combo;
  wlist[5] = changex_spin;
  wlist[6] = changey_spin;
  wlist[7] = changez_spin;
  wlist[8] = plunge_ratio_combo;
  wlist[9] = spindle_rpm_spin;
  wlist[10] = coolant_check_button;
}

static void
image_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_image_t *image;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  image = (gcode_image_t *)block->pdata;

  image->size[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));
  image->size[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  image->size[2] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[4]));

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_image (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *image_tab;
  GtkWidget *alignment;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *sizex_spin;
  GtkWidget *sizey_spin;
  GtkWidget *depthz_spin;
  gcode_t *gcode;
  gcode_image_t *image;
  uint16_t row;

  /**
   * Image Paramters
   */

  gcode = (gcode_t *)block->gcode;

  image = (gcode_image_t *)block->pdata;

  wlist = (GtkWidget **)malloc (5 * sizeof (GtkWidget *));

  row = 0;

  image_tab = gtk_frame_new ("Image Parameters");
  g_signal_connect (image_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), image_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (image_tab), alignment);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  label = gtk_label_new ("Size (X)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, row, row + 1);

  sizex_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (MAX_DIM_X), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sizex_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sizex_spin), image->size[0]);
  g_signal_connect (sizex_spin, "value-changed", G_CALLBACK (image_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), sizex_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Size (Y)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, row, row + 1);

  sizey_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.0), SCALED_INCHES (MAX_DIM_Y), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (sizey_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (sizey_spin), image->size[1]);
  g_signal_connect (sizey_spin, "value-changed", G_CALLBACK (image_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), sizey_spin, 1, 2, row, row + 1);
  row++;

  label = gtk_label_new ("Depth (Z)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, row, row + 1);

  depthz_spin = gtk_spin_button_new_with_range (SCALED_INCHES (-MAX_DIM_Z), SCALED_INCHES (0.0), SCALED_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (depthz_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (depthz_spin), image->size[2]);
  g_signal_connect (depthz_spin, "value-changed", G_CALLBACK (image_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), depthz_spin, 1, 2, row, row + 1);
  row++;

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = sizex_spin;
  wlist[3] = sizey_spin;
  wlist[4] = depthz_spin;
}

static void
stl_update_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  gui_t *gui;
  gcode_block_t *block;
  gcode_stl_t *stl;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  block = (gcode_block_t *)wlist[1];
  stl = (gcode_stl_t *)block->pdata;

  stl->slices = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));

  gcode_stl_generate_slice_contours (block);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);

  update_project_modified_flag (gui, 1);
}

static void
gui_tab_stl (gui_t *gui, gcode_block_t *block)
{
  GtkWidget **wlist;
  GtkWidget *stl_tab;
  GtkWidget *alignment;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *slices_spin;
  gcode_stl_t *stl;
  uint16_t row;

  /**
   * STL Paramters
   */

  stl = (gcode_stl_t *)block->pdata;

  wlist = (GtkWidget **)malloc (3 * sizeof (GtkWidget *));

  row = 0;

  stl_tab = gtk_frame_new ("STL Parameters");
  g_signal_connect (stl_tab, "destroy", G_CALLBACK (generic_destroy_callback), wlist);
  gtk_container_add (GTK_CONTAINER (gui->panel_tab_vbox), stl_tab);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_container_add (GTK_CONTAINER (stl_tab), alignment);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  label = gtk_label_new ("Slices");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, row, row + 1);

  slices_spin = gtk_spin_button_new_with_range (2, 1000, 1);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (slices_spin), 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (slices_spin), stl->slices);
  g_signal_connect (slices_spin, "value-changed", G_CALLBACK (stl_update_callback), wlist);
  gtk_table_attach_defaults (GTK_TABLE (table), slices_spin, 1, 2, row, row + 1);
  row++;

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = (GtkWidget *)block;
  wlist[2] = slices_spin;
}

void
gui_tab_display (gui_t *gui, gcode_block_t *block, int force)
{
  if (block == gui->selected_block && !force)
    return;

  if (gui->panel_tab_vbox)
    gtk_widget_destroy (gui->panel_tab_vbox);

  gui->panel_tab_vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_set_border_width (GTK_CONTAINER (gui->panel_tab_vbox), 1);
  gtk_container_add (GTK_CONTAINER (gui->panel_vbox), gui->panel_tab_vbox);

  if (!block)
    return;

  switch (block->type)
  {
    case GCODE_TYPE_BEGIN:
      gui_tab_begin (gui, block);
      break;

    case GCODE_TYPE_END:
      gui_tab_end (gui, block);
      break;

    case GCODE_TYPE_LINE:
      gui_tab_line (gui, block);
      break;

    case GCODE_TYPE_ARC:
      gui_tab_arc (gui, block);
      break;

    case GCODE_TYPE_BOLT_HOLES:
      gui_tab_bolt_holes (gui, block);
      break;

    case GCODE_TYPE_DRILL_HOLES:
      gui_tab_drill_holes (gui, block);
      break;

    case GCODE_TYPE_POINT:
      gui_tab_point (gui, block);
      break;

    case GCODE_TYPE_TEMPLATE:
      gui_tab_template (gui, block);
      break;

    case GCODE_TYPE_SKETCH:
      gui_tab_sketch (gui, block);
      break;

    case GCODE_TYPE_EXTRUSION:
      gui_tab_extrusion (gui, block);
      break;

    case GCODE_TYPE_TOOL:
      gui_tab_tool (gui, block);
      break;

    case GCODE_TYPE_IMAGE:
      gui_tab_image (gui, block);
      break;

    case GCODE_TYPE_STL:
      gui_tab_stl (gui, block);
      break;

    default:
      break;
  }

  gtk_widget_show_all (gui->panel_vbox);

  gui->opengl.mode = GUI_OPENGL_MODE_EDIT;
  gui->selected_block = block;

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, block);
}
