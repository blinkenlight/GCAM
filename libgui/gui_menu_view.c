/**
 *  gui_menu_view.c
 *  Source code file for G-Code generation, simulation, and visualization
 *  library.
 *
 *  Copyright (C) 2006 - 2010 by Justin Shumaker
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

#include "gui_menu_view.h"
#include "gui.h"
#include "gui_menu_util.h"
#include "gcode.h"

void
gui_menu_view_perspective_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  get_selected_block (gui, &selected_block, &selected_iter);

  gui->opengl.projection = GUI_OPENGL_PROJECTION_PERSPECTIVE;
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_orthographic_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  get_selected_block (gui, &selected_block, &selected_iter);

  gui->opengl.projection = GUI_OPENGL_PROJECTION_ORTHOGRAPHIC;
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_iso_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  GCODE_MATH_VEC3D_SET (gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, 0.0, 0.0, 0.0);
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 45.0;
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].azim = 45.0;

  get_selected_block (gui, &selected_block, &selected_iter);
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_top_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  GCODE_MATH_VEC3D_SET (gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, 0.0, 0.0, 0.0);
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 90.0;
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].azim = 0.0;

  get_selected_block (gui, &selected_block, &selected_iter);
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_left_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  GCODE_MATH_VEC3D_SET (gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, 0.0, 0.0, 0.0);
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 0.0;
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].azim = 90.0;

  get_selected_block (gui, &selected_block, &selected_iter);
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_right_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  GCODE_MATH_VEC3D_SET (gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, 0.0, 0.0, 0.0);
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 0.0;
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].azim = -90.0;

  get_selected_block (gui, &selected_block, &selected_iter);
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_front_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  GCODE_MATH_VEC3D_SET (gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, 0.0, 0.0, 0.0);
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 0.0;
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].azim = 0.0;

  get_selected_block (gui, &selected_block, &selected_iter);
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_back_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkTreeIter selected_iter;
  gui_t *gui;
  gcode_block_t *selected_block;

  gui = (gui_t *)data;

  GCODE_MATH_VEC3D_SET (gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, 0.0, 0.0, 0.0);
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 0.0;
  gui->opengl.views[GUI_OPENGL_VIEW_REGULAR].azim = 180.0;

  get_selected_block (gui, &selected_block, &selected_iter);
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

void
gui_menu_view_render_final_part_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  uint8_t h, m;
  gfloat_t s, time_elapsed;
  char message[128];

  gui = (gui_t *)data;

  if (gui->modified || gui->first_render)
  {
    gui->first_render = 0;
    gcode_render_final (&gui->gcode, &time_elapsed);
    gui_opengl_build_simulate_display_list (&gui->opengl);
  }

  gui->opengl.mode = GUI_OPENGL_MODE_RENDER;

  gui_opengl_context_redraw (&gui->opengl, NULL);

  h = (int)(time_elapsed / 3600.0);
  m = (int)((time_elapsed - (h * 3600)) / 60);
  s = time_elapsed - h * 3600 - m * 60;
  sprintf (message, "Estimated Build Time: %dH %dM %.2f sec", h, m, s);

  update_progress (gui, 0.0);
}
