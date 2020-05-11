/**
 *  gui.c
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

#include "gui.h"
#include "gui_define.h"
#include "gui_endmills.h"
#include "gui_machines.h"
#include "gui_menu.h"
#include "gui_menu_util.h"
#include "gui_tab.h"
#include "gcode.h"
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include "gcam_icon.h"

/* NOT AN EXACT CONVERSION - uses x25 for round values */
#define SCALED_INCHES(x) (GCODE_UNITS ((&gui.gcode), x))

gui_t gui;

static void
gcode_tree_cursor_changed_event (GtkTreeView *treeview, gpointer data)
{
  GtkTreeIter selected_iter;
  gcode_block_t *selected_block;

  get_selected_block (&gui, &selected_block, &selected_iter);
  gui_tab_display (&gui, selected_block, 0);
  update_menu_by_selected_item (&gui, selected_block);
}

static void
gcode_tree_row_collapsed_event (GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, gpointer data)
{
  GtkTreeIter selected_iter;
  gcode_block_t *selected_block;

  get_selected_block (&gui, &selected_block, &selected_iter);

  if (!selected_block)                                                          // If the previously selected path was a descendant of the one just collapsed,
  {                                                                             // 'selected_block' comes back as NULL since the old selection is now invalid;
    set_selected_row_with_iter (&gui, iter);                                    // So in order to avoid a rather awkward "nothing is selected" situation, we
  }                                                                             // use the supplied iter pointing to the collapsed row to select that instead;
}                                                                               // Updating the menu and the tab is done automatically by 'set_selected_row'.

static void
opengl_context_expose_event (GtkWidget *widget, gpointer data)
{
  gcode_block_t *selected_block;
  GtkTreeIter selected_iter;

  get_selected_block (&gui, &selected_block, &selected_iter);

  /**
   * When resizing the exposed event is triggered.
   * It's not guaranteed what order the opengl context callback will be called in.
   * Refresh the opengl context a few times while the rest of the window is being redrawn.
   * This is kind of nasty, but it's the only solution I can think of right now without a using timer.
   */

  gui_opengl_context_redraw (&gui.opengl, selected_block);
  gui_opengl_context_redraw (&gui.opengl, selected_block);
  gui_opengl_context_redraw (&gui.opengl, selected_block);
}

static gboolean
opengl_context_key_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  return (TRUE);
}

static gboolean
opengl_context_button_event (GtkWidget *widget, GdkEventButton *event)
{
  gcode_block_t *selected_block;
  GtkTreeIter selected_iter;
  guint modifiers;

  get_selected_block (&gui, &selected_block, &selected_iter);

  if (!selected_block)
    return (TRUE);

  modifiers = gtk_accelerator_get_default_mod_mask ();

  /* Accurate to within 1/100th of a second, good 'nuff for frame dropping */
  gui.event_time = 0.001 * (double)gtk_get_current_event_time () - g_timer_elapsed (gui.timer, NULL);

  gui.mouse_x = event->x;
  gui.mouse_y = event->y;

  if (event->type == GDK_BUTTON_RELEASE)
  {
    if (event->button == 3)
    {
      gfloat_t z;
      int16_t dx, dy;

      dx = (int16_t) (event->x - gui.mouse_x);
      dy = (int16_t) (event->y - gui.mouse_y);

      z = dy < 0 ? 0.985 : 1.015;
      z = pow (z, fabs (dy));

      if (gui.opengl.view == GUI_OPENGL_VIEW_REGULAR)
      {
        if (gui.opengl.projection == GUI_OPENGL_PROJECTION_PERSPECTIVE)
        {
          gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom *= z;

          if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
            gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

          if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
            gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
        }
        else
        {
          gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid *= z;

          if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
            gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

          if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
            gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
        }
      }
      else if (gui.opengl.view == GUI_OPENGL_VIEW_EXTRUSION)
      {
        gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid *= z;

        if (gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
          gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

        if (gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
          gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
      }
    }

    gui_opengl_context_redraw (&gui.opengl, selected_block);
  }
  else if (event->type == GDK_BUTTON_PRESS)
  {
    if (event->button == 1 && (event->state & modifiers) == GDK_CONTROL_MASK)
      gui_opengl_pick (&gui.opengl, event->x, event->y);
  }

  return (TRUE);
}

static gboolean
opengl_context_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
  int16_t dx, dy;
  uint8_t update;
  gfloat_t delay;
  gcode_block_t *selected_block;
  GtkTreeIter selected_iter;
  guint modifiers;

  get_selected_block (&gui, &selected_block, &selected_iter);

  if (!selected_block)
    return (TRUE);

  modifiers = gtk_accelerator_get_default_mod_mask ();

  update = 0;

  dx = (int16_t) (event->x - gui.mouse_x);
  dy = (int16_t) (event->y - gui.mouse_y);

  if (event->state & GDK_BUTTON1_MASK && (event->state & modifiers) != GDK_CONTROL_MASK)
  {
    gfloat_t celev, delta[2], coef;

    coef = (2.0 / (gfloat_t)gui.opengl.context_w);

    if (gui.opengl.view == GUI_OPENGL_VIEW_REGULAR)
    {
      gcode_vec3d_t up, side, look;

      /* relative up/down left/right movement */
      GCODE_MATH_VEC3D_SET (up, 0.0, 0.0, 1.0);

      celev = cos (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].elev * GCODE_DEG2RAD);
      look[0] = -cos ((gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].azim + 90.0) * GCODE_DEG2RAD) * celev;
      look[1] = -sin (-(gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].azim + 90.0) * GCODE_DEG2RAD) * celev;
      look[2] = -sin (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].elev * GCODE_DEG2RAD);
      GCODE_MATH_VEC3D_UNITIZE (look);

      /* side vector */
      GCODE_MATH_VEC3D_CROSS (side, look, up);
      GCODE_MATH_VEC3D_UNITIZE (side);

      /* up vector */
      GCODE_MATH_VEC3D_CROSS (up, look, side);
      GCODE_MATH_VEC3D_UNITIZE (up);

      if (gui.opengl.projection == GUI_OPENGL_PROJECTION_PERSPECTIVE)
      {
        delta[0] = -coef * gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom * dx;
        delta[1] = -coef * gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom * dy;
      }
      else
      {
        delta[0] = -coef * gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid * dx;
        delta[1] = -coef * gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid * dy;
      }

      /* left/right */
      GCODE_MATH_VEC3D_MUL_SCALAR (side, side, delta[0]);
      GCODE_MATH_VEC3D_ADD (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, side);

      /* up/down */
      GCODE_MATH_VEC3D_MUL_SCALAR (up, up, delta[1]);
      GCODE_MATH_VEC3D_ADD (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].pos, up);
    }
    else if (gui.opengl.view == GUI_OPENGL_VIEW_EXTRUSION)
    {
      gcode_vec3d_t pan;

      delta[0] = -coef * gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid * dx;
      delta[1] = -coef * gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid * dy;

      GCODE_MATH_VEC3D_SET (pan, delta[0], -delta[1], 0);
      GCODE_MATH_VEC3D_ADD (gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].pos, gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].pos, pan);
    }

    update = 1;
  }

  if (event->state & GDK_BUTTON2_MASK)
  {
    if (gui.opengl.view == GUI_OPENGL_VIEW_REGULAR)
    {
      gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].azim += (dx * 0.5);
      gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].elev += (dy * 0.5);

      if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].elev < 0.0)
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 0.0;

      if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].elev > 90.0)
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].elev = 90.0;

      update = 1;
    }
  }

  if (event->state & GDK_BUTTON3_MASK)
  {
    gfloat_t z;

    z = dy < 0 ? 0.99 : 1.01;
    z = pow (z, fabs (dy));

    if (gui.opengl.view == GUI_OPENGL_VIEW_REGULAR)
    {
      if (gui.opengl.projection == GUI_OPENGL_PROJECTION_PERSPECTIVE)
      {
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom *= z;

        if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
          gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

        if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
          gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
      }
      else
      {
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid *= z;

        if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
          gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

        if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
          gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
      }
    }
    else if (gui.opengl.view == GUI_OPENGL_VIEW_EXTRUSION)
    {
      gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid *= z;

      if (gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
        gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

      if (gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
        gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
    }

    update = 1;
  }

  delay = (gui.event_time + g_timer_elapsed (gui.timer, NULL)) - 0.001 * (double)gtk_get_current_event_time ();

  /* Drop frames that are older than 20ms old */
  if (update && delay < 0.02)
    gui_opengl_context_redraw (&gui.opengl, selected_block);

  gui.mouse_x = event->x;
  gui.mouse_y = event->y;

  return (TRUE);
}

static gboolean
opengl_context_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
  gcode_block_t *selected_block;
  GtkTreeIter selected_iter;
  gfloat_t z;

  get_selected_block (&gui, &selected_block, &selected_iter);

  if (!selected_block)
    return (TRUE);

  z = event->direction ? 1.1 : 0.9;

  if (gui.opengl.view == GUI_OPENGL_VIEW_REGULAR)
  {
    if (gui.opengl.projection == GUI_OPENGL_PROJECTION_PERSPECTIVE)
    {
      gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom *= z;

      if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

      if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].zoom = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
    }
    else
    {
      gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid *= z;

      if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

      if (gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
        gui.opengl.views[GUI_OPENGL_VIEW_REGULAR].grid = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
    }
  }
  else if (gui.opengl.view == GUI_OPENGL_VIEW_EXTRUSION)
  {
    gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid *= z;

    if (gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid > SCALED_INCHES (GUI_OPENGL_MAX_ZOOM))
      gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid = SCALED_INCHES (GUI_OPENGL_MAX_ZOOM);

    if (gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid < SCALED_INCHES (GUI_OPENGL_MIN_ZOOM))
      gui.opengl.views[GUI_OPENGL_VIEW_EXTRUSION].grid = SCALED_INCHES (GUI_OPENGL_MIN_ZOOM);
  }

  gui_opengl_context_redraw (&gui.opengl, selected_block);

  return (TRUE);
}

static void
suppress_toggled (GtkCellRendererToggle *cell, gchar *path_string, gpointer data)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gcode_block_t *selected_block;
  gboolean toggle_item;
  gint *column;

  tree_view = GTK_TREE_VIEW (gui.gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  path = gtk_tree_path_new_from_string (path_string);

  column = g_object_get_data (G_OBJECT (cell), "column");

  /* get toggled iter */
  gtk_tree_model_get_iter (tree_model, &iter, path);
  set_selected_row_with_iter (&gui, &iter);                                     /* This is done because the selected block is currently the old one */
  get_selected_block (&gui, &selected_block, &iter);

  if (gui.ignore_signals || selected_block->flags & GCODE_FLAGS_LOCK)
    return;

  gtk_tree_model_get (tree_model, &iter, column, &toggle_item, -1);

  /* do something with the value */
  toggle_item ^= 1;

  /* set new value */
  gtk_tree_store_set (GTK_TREE_STORE (tree_model), &iter, column, toggle_item, -1);

  /* clean up */
  gtk_tree_path_free (path);

  /* update block flag bits */
  selected_block->flags = (selected_block->flags & ~GCODE_FLAGS_SUPPRESS) | toggle_item << 1;

  /* Update OpenGL context */
  gui.opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui.opengl, selected_block);

  update_project_modified_flag (&gui, 1);
}

static void
comment_cell_edited (GtkCellRendererText *cell, gchar *path_string, gchar *new_text, gpointer data)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gcode_block_t *selected_block;
  int i, j, k;
  char *modified_text;

  tree_view = GTK_TREE_VIEW (gui.gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  path = gtk_tree_path_new_from_string (path_string);

  gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

  gtk_tree_model_get_iter (tree_model, &iter, path);

  /* Replace certain characters in string */
  k = sizeof(((gcode_block_t *)0)->comment);                                    // Get the size of a gcode block's comment field: it is not to be exceeded
  modified_text = malloc (k);
  j = 0;

  for (i = 0; i < k - 1; i++)
  {
    modified_text[j] = new_text[i];

    if (new_text[i] == '(')
      modified_text[j] = '[';

    if (new_text[i] == ')')
      modified_text[j] = ']';

    if (new_text[i] == ';')
      j--;

    j++;
  }

  modified_text[j] = 0;                                                         /* Null terminate the string */

  gtk_tree_store_set (GTK_TREE_STORE (tree_model), &iter, column, modified_text, -1);

  /* Update the gcode block too. */
  get_selected_block (&gui, &selected_block, &iter);

  if (selected_block)
    strcpy (selected_block->comment, modified_text);

  gtk_tree_path_free (path);
  free (modified_text);

  update_project_modified_flag (&gui, 1);
}

/**
 * As part of our custom tree view 'drag source' interface, 'row_draggable' must
 * decide whether the selected row is draggable or not, by returning TRUE/FALSE.
 */

static gboolean
row_draggable (GtkTreeDragSource *drag_source, GtkTreePath *source_path)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeIter iter;
  GValue value = { 0, };
  gcode_block_t *source_block = NULL;

  tree_view = GTK_TREE_VIEW (gui.gcode_block_treeview);                         // For lack of a better way, we retrieve our stashed reference to TreeView;

  tree_model = gtk_tree_view_get_model (tree_view);                             // We need a reference to TreeView's tree model;

  if (gtk_tree_model_get_iter (tree_model, &iter, source_path))                 // Using that and the supplied path we can get an iter for the dragged row;
  {
    gtk_tree_model_get_value (tree_model, &iter, 5, &value);                    // Using the iter, we can fetch the content of the fifth column of that row,
    source_block = (gcode_block_t *)g_value_get_pointer (&value);               // to retrieve the pointer to the associated block;
    g_value_unset (&value);
  }

  if (!source_block)                                                            // If the source block reference is still NULL:
    return (FALSE);                                                             // let's just conclude it's NOT draggable.

  if (source_block->flags & GCODE_FLAGS_LOCK)                                   // If the source block is locked:
    return (FALSE);                                                             // yeah, you guessed it - NOT draggable.

  return (TRUE);                                                                // What, we're still here...? Fine, then I suppose that row IS draggable...
}

/**
 * As part of our custom 'drag destination' interface, 'row_drop_possible' must
 * decide whether dropping on this target is OK or not, by returning TRUE/FALSE.
 * NOTE: I consider the provided 'dest_path' unsuitable for its purpose because
 * denying a drop INTO a row will automatically result in a new request to drop
 * BEFORE or AFTER it (which might be perfectly valid drop targets so might get
 * allowed), while the GUI still shows the original (invalid/denied) INTO drop;
 * While the operation would proceed correctly, this behaviour results in stuff
 * getting dropped into other places than the user intended to drop them based
 * on the GUI feedback - there's no way to tell 'dest_path' is not the intended
 * drop spot; hence, we retrieve the data via 'gtk_tree_view_get_drag_dest_row'
 * but since that returns NULL in the actual drop execution handler, we have to
 * preserve the drop path and drop spot quite inelegantly in a member of 'gui'.
 */

static gboolean
row_drop_possible (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreePath *source_path;                                                     // This does need freeing, it points to a new path created for us;
  GtkTreePath *target_path;                                                     // This does not need freeing - it's not created specially for us;
  GtkTreePath *parent_path;                                                     // This does need freeing, it points to a new path created for us;
  GtkTreePath *prev_path;                                                       // This does need freeing, it points to a new path created for us;
  GtkTreePath *next_path;                                                       // This does need freeing, it points to a new path created for us;
  GtkTreeIter iter;
  GValue value = { 0, };
  gboolean drop_allowed = TRUE;
  gcode_block_t *source_block = NULL;
  gcode_block_t *target_block = NULL;
  gcode_block_t *parent_block = NULL;
  GtkTreeViewDropPosition target_spot;

  tree_view = GTK_TREE_VIEW (gui.gcode_block_treeview);                         // For lack of a better way, we retrieve our stashed reference to TreeView;

  gtk_tree_get_row_drag_data (selection_data, &tree_model, &source_path);       // Using the selection data, we can get the tree model and the source path

  if (gtk_tree_model_get_iter (tree_model, &iter, source_path))                 // Using those we can get an iter for the source (the dragged row);
  {
    gtk_tree_model_get_value (tree_model, &iter, 5, &value);                    // Using the iter, we can fetch the content of the fifth column of that row,
    source_block = (gcode_block_t *)g_value_get_pointer (&value);               // to retrieve the pointer to the associated block;
    g_value_unset (&value);
  }

  if (!source_block)                                                            // If the source block reference is still NULL:
  {
    gtk_tree_path_free (source_path);
    return (FALSE);                                                             // let's just conclude this drop isn't happening.
  }

  gtk_tree_view_get_drag_dest_row (tree_view, &target_path, &target_spot);      // Since 'dest_past' is useless, we retrieve the drop path and spot this way;

  if (!target_path)                                                             // Most amusingly, on some target rows (not all!) we get an extra, final event
  {                                                                             // when the drop itself happens, and on those occasions this returns a target
    gtk_tree_path_free (source_path);                                           // path of NULL in spite of the fact that there CLEARLY IS a target selected;
    return (FALSE);                                                             // Even more amusingly, it doesn't even matter that we return FALSE for these
  }                                                                             // (we leave to protect 'row_drop_path'), the drop handler gets called anyway.

  if (gui.row_drop_path)                                                        // If 'row_drop_path' exists, we should free it before creating a new path;
    gtk_tree_path_free (gui.row_drop_path);

  gui.row_drop_path = gtk_tree_path_copy (target_path);                         // Now we're free to make a copy of the drop target path and stash it away,
  gui.row_drop_spot = target_spot;                                              // along with the drop target spot, for lack of a better way, into 'gui'.

  if (gtk_tree_model_get_iter (tree_model, &iter, target_path))                 // Using those we can get an iter for the target (the row to drop onto);
  {
    gtk_tree_model_get_value (tree_model, &iter, 5, &value);                    // Using the iter, we can fetch the content of the fifth column of that row,
    target_block = (gcode_block_t *)g_value_get_pointer (&value);               // to retrieve the pointer to the associated block;
    g_value_unset (&value);
  }

  if (!target_block)                                                            // If the target block reference is still NULL:
  {
    gtk_tree_path_free (source_path);
    return (FALSE);                                                             // let's just conclude this drop isn't happening.
  }

  parent_path = gtk_tree_path_copy (target_path);                               // Make a copy of the drop target path then see if the drop is INTO it or not;

  if ((target_spot == GTK_TREE_VIEW_DROP_BEFORE) || (target_spot == GTK_TREE_VIEW_DROP_AFTER))
  {
    if (gtk_tree_path_get_depth (target_path) > 1)                              // If there's at least a second level in that path (it's not top-level),
    {                                                                           // try to move up one level on it;
      gtk_tree_path_up (parent_path);                                           // The result should be tested but it's pointless: it's broken (always TRUE);

      if (gtk_tree_model_get_iter (tree_model, &iter, parent_path))             // If we can get a valid iter for the parent node,
      {
        gtk_tree_model_get_value (tree_model, &iter, 5, &value);                // fetch the content of the fifth column of that row,
        parent_block = (gcode_block_t *)g_value_get_pointer (&value);           // to retrieve the pointer to the associated block;
        g_value_unset (&value);
      }
      else                                                                      // If there's no iter for the parent path, this drop is not going to work;
        drop_allowed = FALSE;
    }                                                                           // If the target path depth is one, it's top level: 'parent_block' stays NULL.
  }
  else                                                                          // If the drop is INTO the target, the new parent is the target itself;
    parent_block = target_block;

  /* Generic interdictions */

  if (source_block->flags & GCODE_FLAGS_LOCK)                                   // Yeah, it's already checked: but if the source is locked, definitely no drop;
    drop_allowed = FALSE;

  if (gtk_tree_path_compare (target_path, source_path) == 0)                    // Don't drop into self
    drop_allowed = FALSE;

  if (gtk_tree_path_is_descendant (target_path, source_path))                   // Don't drop under self
    drop_allowed = FALSE;

  prev_path = gtk_tree_path_copy (source_path);                                 // Make a copy of the source path then find the path of the previous item

  if (gtk_tree_path_prev (prev_path))                                           // Don't drop right after the previous item - that would be no change;
    if (gtk_tree_path_compare (target_path, prev_path) == 0)                    // Note: thanks a lot for having no "get previous iter" (only "get next iter") in GTK2
      if (target_spot == GTK_TREE_VIEW_DROP_AFTER)
        drop_allowed = FALSE;

  next_path = gtk_tree_path_copy (source_path);                                 // Make a copy of the source path then find the path of the next item
  gtk_tree_path_next (next_path);                                               // Note: also thanks a lot for there being no feedback at all whether "next" for a path actually exists or not

  if (gtk_tree_model_get_iter (tree_model, &iter, next_path))                   // Don't drop right before the next item - that would be no change;
    if (gtk_tree_path_compare (target_path, next_path) == 0)                    // (the iter is only requested to check if the path actually exists)
      if (target_spot == GTK_TREE_VIEW_DROP_BEFORE)
        drop_allowed = FALSE;

  if (!parent_block)                                                            // If top level (no parent), check validity as a top level block;
  {
    if (!GCODE_IS_VALID_IF_NO_PARENT[source_block->type])
      drop_allowed = FALSE;
  }
  else                                                                          // If not top level (parent exists), check parent-child validity;
  {
    if (!GCODE_IS_VALID_PARENT_CHILD[parent_block->type][source_block->type])
      drop_allowed = FALSE;
  }

  /* Specific interdictions */

  if ((target_block->type == GCODE_TYPE_BEGIN) && (target_spot == GTK_TREE_VIEW_DROP_BEFORE))   // Don't drop before a BEGIN block
    drop_allowed = FALSE;

  if ((target_block->type == GCODE_TYPE_END) && (target_spot == GTK_TREE_VIEW_DROP_AFTER))      // Don't drop after an END block
    drop_allowed = FALSE;

  if ((target_block->type == GCODE_TYPE_EXTRUSION) && (target_spot == GTK_TREE_VIEW_DROP_BEFORE))       // Don't drop before an EXTRUSION block
    drop_allowed = FALSE;

  if ((target_block->type == GCODE_TYPE_EXTRUSION) && (target_spot == GTK_TREE_VIEW_DROP_AFTER))        // Don't drop after an EXTRUSION block
    drop_allowed = FALSE;

  gtk_tree_path_free (source_path);                                             // Free all the involved paths;
  gtk_tree_path_free (parent_path);                                             // They are the reason we don't simply "return" on every fail.
  gtk_tree_path_free (prev_path);
  gtk_tree_path_free (next_path);

  return (drop_allowed);
}

/**
 * As part of our custom 'drag destination' interface, 'drag_data_received' must
 * execute the insertion of the dropped row in the tree store at the target spot;
 * I believe removal of the dragged row from its original location in the tree
 * store is NOT supposed to be performed here, but the original source code does
 * it anyway - therefore SO SHALL WE, end of story. Either way, the return value
 * should be in sync with what we do - if we delete the row, it should be TRUE,
 * if we don't, it should be FALSE. On the other hand, the burden of duplicating
 * the effect on the underlying block structure obviously falls entirely on us:
 * we must both remove and re-insert the moved block from/into the block tree.
 * NOTE: For reasons outlined in the previous handler, I prefer not to rely on
 * the provided 'dest_path' to determine the drop target location; instead, we
 * retrieve the drop path and drop spot (saved earlier) from a member of 'gui'.
 */

static gboolean
drag_data_received (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
  GtkTreeModel *tree_model;
  GtkTreePath *source_path;
  GtkTreePath *target_path;
  GtkTreeIter source_iter;
  GtkTreeIter target_iter;
  GValue value = { 0, };
  gcode_block_t *source_block;
  gcode_block_t *target_block;
  GtkTreeViewDropPosition target_spot;
  int insert_spot;

  if (!gui.row_drop_path)                                                       // If 'row_drop_path' does not exists, we just don't want to play anymore;
    return (FALSE);

  target_path = gui.row_drop_path;                                              // Retrieve our stashed copies of the drop path and drop spot
  target_spot = gui.row_drop_spot;

  gui.row_drop_path = NULL;                                                     // The path shall be freed before leaving but that's no longer gui's concern;

  gtk_tree_get_row_drag_data (selection_data, &tree_model, &source_path);       // Using the selection data, we can get the tree model and the source path

  if (gtk_tree_model_get_iter (tree_model, &source_iter, source_path))          // Using those we can get an iter for the source (the dragged row);
  {
    gtk_tree_model_get_value (tree_model, &source_iter, 5, &value);             // Using the iter, we can fetch the content of the fifth column of that row,
    source_block = (gcode_block_t *)g_value_get_pointer (&value);               // to retrieve the pointer to the associated block;
    g_value_unset (&value);
  }
  else                                                                          // If there's no valid iter for the dragged row,
  {                                                                             // let's just conclude this drop isn't happening;
    gtk_tree_path_free (source_path);                                           // The source and target path should be freed before leaving.
    gtk_tree_path_free (target_path);
    return (FALSE);
  }

  if (gtk_tree_model_get_iter (tree_model, &target_iter, target_path))          // Using those we can get an iter for the target (the row dropped on);
  {
    gtk_tree_model_get_value (tree_model, &target_iter, 5, &value);             // Using the iter, we can fetch the content of the fifth column of that row,
    target_block = (gcode_block_t *)g_value_get_pointer (&value);               // to retrieve the pointer to the associated block;
    g_value_unset (&value);
  }
  else                                                                          // If there's no valid iter for the drop target row,
  {                                                                             // let's just conclude this drop isn't happening;
    gtk_tree_path_free (source_path);                                           // The source and target path should be freed before leaving.
    gtk_tree_path_free (target_path);
    return (FALSE);
  }

  if ((!source_block) || (!target_block))                                       // If we got valid iters this is quite unlikely - but the blocks must exist too
  {
    gtk_tree_path_free (source_path);                                           // The source and target path should be freed before leaving.
    gtk_tree_path_free (target_path);
    return (FALSE);
  }

  /**
   * We have to do some consolidating - target spot can be either BEFORE, AFTER
   * or INTO, but the insert logic accepts only either AFTER or UNDER, so BEFORE
   * has to be converted into "AFTER previous" or "UNDER parent" depending on the
   * existence or lack of another block before the target one. But as much as we
   * prefer to work on actual tree store data instead of blocks when the GUI is
   * involved, extrusions complicate the concept of "previous element", so this
   * time we check this using the blocks and not the paths / iters.
   */

  switch (target_spot)
  {
    case GTK_TREE_VIEW_DROP_BEFORE:                                             // This will not do - convert to one of the other two drop spots;

      if (target_block->prev)                                                   // If there is a previous block,
      {
        insert_spot = GUI_INSERT_AFTER;                                         // set 'insert_spot' to AFTER,
        gtk_tree_path_prev (target_path);                                       // and move the target path to the previous row.
      }
      else                                                                      // If there is no previous block,
      {
        insert_spot = GUI_INSERT_UNDER;                                         // set 'insert_spot' to UNDER,
        gtk_tree_path_up (target_path);                                         // and move the target path to the parent row.
      }

      if (gtk_tree_model_get_iter (tree_model, &target_iter, target_path))      // Now we have to re-acquire the target iter and the target block;
      {
        gtk_tree_model_get_value (tree_model, &target_iter, 5, &value);         // Using the iter, we can fetch the content of the fifth column of that row,
        target_block = (gcode_block_t *)g_value_get_pointer (&value);           // to retrieve the pointer to the associated block;
        g_value_unset (&value);
      }
      else                                                                      // This should not happen, but if the updated target iter is not valid,
      {                                                                         // throw in the towel and exit stage left;
        gtk_tree_path_free (source_path);                                       // The source and target path should be freed before leaving.
        gtk_tree_path_free (target_path);
        return (FALSE);
      }

      break;

    case GTK_TREE_VIEW_DROP_AFTER:                                              // There's not much to do if it's a straight AFTER drop;

      insert_spot = GUI_INSERT_AFTER;                                           // 'target_iter' and 'target_block' are already valid, just set 'insert_spot';

      break;

    default:                                                                    // If it's neither BEFORE nor AFTER then it's INTO, end of story.

      insert_spot = GUI_INSERT_UNDER;                                           // 'target_iter' and 'target_block' are already valid, just set 'insert_spot';

      break;
  }

  remove_primitive (&gui, source_block, &source_iter);                          // Remove the 'source' row both from the tree store and the block tree;
  insert_primitive (&gui, source_block, target_block, &target_iter, insert_spot);       // Now put it right back under/after both the 'target' row and block.

  gui.opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui.opengl, target_block);

  update_project_modified_flag (&gui, 1);

  gtk_tree_path_free (source_path);                                             // The source and target path should be freed before leaving.
  gtk_tree_path_free (target_path);

  return (TRUE);
}

/**
 * Reconnect a gcode object wiped clean by an 'init' action to a gui object
 */

void
gui_attach (gcode_t *gcode, gui_t *gui)
{
  gcode->gui = gui;

  gcode->progress_callback = update_progress;
  gcode->message_callback = generic_dialog;

  gcode->voxel_resolution = gui->settings.voxel_resolution;
  gcode->curve_segments = gui->settings.curve_segments;
  gcode->roughing_overlap = gui->settings.roughing_overlap;
  gcode->padding_fraction = gui->settings.padding_fraction;
}

/**
 * Callback installed for the "close window clicked" event of the main window;
 * Without it, a click on "X" and any modified but unsaved project goes "poof".
 */

static gboolean
gui_close (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  gui_menu_file_quit_menuitem_callback (NULL, data);                            // Call the handler for the "quit" menu, so it can offer a chance to save;
  return (TRUE);                                                                // Return TRUE to cancel immediate GUI destruction: the handler needs time!
}

void
gui_init (char *filename)
{
  GdkGLConfig *glconfig;
  GtkWidget *window_hbox;
  GtkWidget *window_vbox_main;
  GtkWidget *window_vbox_minor;
  GtkWidget *window_vbox_right;
  GtkWidget *gl_context;
  char *fatal_message;

  gui.project_state = PROJECT_CLOSED;
  gui.selected_block = NULL;
  strcpy (gui.filename, "");
  gui.panel_tab_vbox = NULL;
  gui.modified = 0;
  strcpy (gui.current_folder, "");
  gui.ignore_signals = 0;
  gui.first_render = 1;

  fatal_message = NULL;

  gui_settings_init (&gui.settings);

  if (gui_settings_read (&gui.settings) != 0)
    fatal_message = (char *)GCAM_FATAL_SETTINGS;

  gui_machines_init (&gui.machines);

  if (gui_machines_read (&gui.machines) != 0)
    fatal_message = (char *)GCAM_FATAL_MACHINES;

  gui_endmills_init (&gui.endmills);

  if (gui_endmills_read (&gui.endmills) != 0)
    fatal_message = (char *)GCAM_FATAL_ENDMILLS;

  /**
   * Opengl Display Defaults
   */

  gui.opengl.context_w = WINDOW_W;
  gui.opengl.context_h = WINDOW_H - PANEL_BOTTOM_H;
  gui.opengl.gcode = &gui.gcode;
  gui.opengl.ready = 0;
  gui.opengl.projection = GUI_OPENGL_PROJECTION_ORTHOGRAPHIC;
  gui.opengl.progress_callback = &update_progress;

  gui.timer = g_timer_new ();
  g_timer_start (gui.timer);

  /* Initialize GTK */
  gtk_init (0, 0);
  gtk_gl_init (0, 0);

  glconfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGBA | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE);

  /* Create Main Window */
  gui.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (GTK_WIDGET (gui.window), WINDOW_W, WINDOW_H);
  gtk_window_set_icon (GTK_WINDOW (gui.window), gdk_pixbuf_new_from_xpm_data (gcam_icon_xpm));

  /* Set Window Title */
  update_project_modified_flag (&gui, 0);

  g_signal_connect (G_OBJECT (gui.window), "delete-event", G_CALLBACK (gui_close), &gui);
  g_signal_connect (G_OBJECT (gui.window), "destroy", G_CALLBACK (gui_destroy), NULL);

  /* Create a vbox for the opengl context and code block tree */
  window_vbox_main = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (window_vbox_main), 0);
  gtk_container_add (GTK_CONTAINER (gui.window), window_vbox_main);

  /* Create menu bar */
  {
    GtkIconFactory *icon_factory;
    GtkActionGroup *action_group;
    GtkAccelGroup *accel_group;
    GtkWidget *menubar;
    GError *error;

    /* Create menu icons */

    icon_factory = gtk_icon_factory_new ();

    for (int i = 0; icon_table[i].xpm_data && icon_table[i].stock_id; i++)
    {
      GdkPixbuf *pixbuf;
      GtkIconSet *icon_set;

      pixbuf = gdk_pixbuf_new_from_xpm_data (icon_table[i].xpm_data);
      icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (icon_factory, icon_table[i].stock_id, icon_set);
      g_object_unref (pixbuf);
    }

    gtk_icon_factory_add_default (icon_factory);

    /* Create the application menu */

    action_group = gtk_action_group_new ("MenuActions");
    gtk_action_group_add_actions (action_group, gui_menu_entries, G_N_ELEMENTS (gui_menu_entries), &gui);
    gui.ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (gui.ui_manager, action_group, 0);

    accel_group = gtk_ui_manager_get_accel_group (gui.ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (gui.window), accel_group);

    error = NULL;

    if (!gtk_ui_manager_add_ui_from_string (gui.ui_manager, gui_menu_description, -1, &error))
    {
      g_message ("building menus failed: %s", error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

    menubar = gtk_ui_manager_get_widget (gui.ui_manager, "/MainMenu");
    gtk_box_pack_start (GTK_BOX (window_vbox_main), menubar, FALSE, FALSE, 0);

    /* Disable certain widgets until a project is being worked on */
    update_menu_by_project_state (&gui, PROJECT_CLOSED);

    /* Disable Save until Save As is used. */
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui.ui_manager, "/MainMenu/FileMenu/Save"), 0);
  }

  /* Create a sub-vbox for the hbox and the progress bar */
  window_vbox_minor = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (window_vbox_minor), 2);
  gtk_box_pack_end (GTK_BOX (window_vbox_main), window_vbox_minor, TRUE, TRUE, 0);

  /* Create an hbox for left panel and opengl context */
  window_hbox = gtk_hbox_new (FALSE, 1);
  gtk_container_set_border_width (GTK_CONTAINER (window_hbox), 0);
  gtk_box_pack_start (GTK_BOX (window_vbox_minor), window_hbox, TRUE, TRUE, 0);

  /* Create a vbox for the Left Panel */
  gui.panel_vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_set_border_width (GTK_CONTAINER (gui.panel_vbox), 0);
  gtk_box_pack_start (GTK_BOX (window_hbox), gui.panel_vbox, FALSE, FALSE, 0);
  gtk_widget_set_size_request (GTK_WIDGET (gui.panel_vbox), PANEL_LEFT_W, 0);

  /* Create a vbox for the opengl context and block tree on the right side of the window */
  window_vbox_right = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (window_vbox_right), 0);
  gtk_box_pack_end (GTK_BOX (window_hbox), window_vbox_right, TRUE, TRUE, 0);

  /* Create the gl context */
  gl_context = gtk_drawing_area_new ();
  gtk_box_pack_start (GTK_BOX (window_vbox_right), gl_context, TRUE, TRUE, 0);

  /* Give gl-capability to the widget. */
  gtk_widget_set_gl_capability (gl_context, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);

  gtk_widget_add_events (gl_context, GDK_VISIBILITY_NOTIFY_MASK);
  g_signal_connect (G_OBJECT (gl_context), "configure_event", G_CALLBACK (gui_opengl_context_init), &gui.opengl);
  g_signal_connect (G_OBJECT (gl_context), "expose_event", G_CALLBACK (opengl_context_expose_event), NULL);
  g_signal_connect (G_OBJECT (gl_context), "key_press_event", G_CALLBACK (opengl_context_key_event), NULL);
  g_signal_connect (G_OBJECT (gl_context), "button_press_event", G_CALLBACK (opengl_context_button_event), NULL);
  g_signal_connect (G_OBJECT (gl_context), "button_release_event", G_CALLBACK (opengl_context_button_event), NULL);
  g_signal_connect (G_OBJECT (gl_context), "motion_notify_event", G_CALLBACK (opengl_context_motion_event), NULL);
  g_signal_connect (G_OBJECT (gl_context), "scroll_event", G_CALLBACK (opengl_context_scroll_event), NULL);

  gtk_widget_set_events (gl_context,
                         GDK_EXPOSURE_MASK |
                         GDK_LEAVE_NOTIFY_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_SCROLL_MASK |
                         GDK_POINTER_MOTION_MASK);

  /* Code Block Tree List */
  {
    GtkWidget *sw;
    GtkTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_end (GTK_BOX (window_vbox_right), sw, FALSE, FALSE, 0);

    /* Make room for horizontal scroll bar */
    gtk_widget_set_size_request (GTK_WIDGET (sw), WINDOW_W, PANEL_BOTTOM_H - 28);

    /* Create Code Block Tree List */
    gui.gcode_block_store = gtk_tree_store_new (6, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
    model = GTK_TREE_MODEL (gui.gcode_block_store);

    /* create tree view */
    gui.gcode_block_treeview = gtk_tree_view_new_with_model (model);

    g_object_unref (model);

#if GTK_MAJOR_VERSION >= 2
#if GTK_MINOR_VERSION >= 10
    gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (gui.gcode_block_treeview), 1);
#endif
#endif

    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (gui.gcode_block_treeview), TRUE);
    gtk_tree_view_set_search_column (GTK_TREE_VIEW (gui.gcode_block_treeview), 0);
    gtk_tree_view_set_reorderable (GTK_TREE_VIEW (gui.gcode_block_treeview), TRUE);
    GTK_TREE_DRAG_SOURCE_GET_IFACE (gui.gcode_block_store)->row_draggable = row_draggable;
    GTK_TREE_DRAG_DEST_GET_IFACE (gui.gcode_block_store)->row_drop_possible = row_drop_possible;
    GTK_TREE_DRAG_DEST_GET_IFACE (gui.gcode_block_store)->drag_data_received = drag_data_received;
    g_signal_connect (G_OBJECT (gui.gcode_block_treeview), "cursor-changed", G_CALLBACK (gcode_tree_cursor_changed_event), NULL);
    g_signal_connect (G_OBJECT (gui.gcode_block_treeview), "row-collapsed", G_CALLBACK (gcode_tree_row_collapsed_event), NULL);
    gtk_container_add (GTK_CONTAINER (sw), gui.gcode_block_treeview);

    /* ORDER COLUMN */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Order", renderer, "text", 0, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gui.gcode_block_treeview), column);

    /* TYPE COLUMN */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Type", renderer, "text", 1, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gui.gcode_block_treeview), column);

    /* STATUS COLUMN */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_renderer_set_alignment (renderer, 0.5, 0.5);
    column = gtk_tree_view_column_new_with_attributes ("Status", renderer, "text", 2, NULL);
    gtk_tree_view_column_set_alignment (column, 0.5);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gui.gcode_block_treeview), column);

    /* SUPPRESS COLUMN */
    renderer = gtk_cell_renderer_toggle_new ();
    gtk_cell_renderer_set_alignment (renderer, 0.5, 0.5);
    g_object_set_data (G_OBJECT (renderer), "column", (gint *) 3);
    column = gtk_tree_view_column_new_with_attributes ("Suppress", renderer, "active", 3, NULL);
    gtk_tree_view_column_set_alignment (column, 0.5);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gui.gcode_block_treeview), column);
    g_signal_connect (renderer, "toggled", G_CALLBACK (suppress_toggled), NULL);

    /* COMMENT COLUMN */
    renderer = gtk_cell_renderer_text_new ();
    gui.comment_cell = renderer;
    g_object_set (renderer, "editable", TRUE, NULL);
    g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (4));
    column = gtk_tree_view_column_new_with_attributes ("Comment", renderer, "text", 4, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gui.gcode_block_treeview), column);
    g_signal_connect (renderer, "edited", G_CALLBACK (comment_cell_edited), NULL);

    /* Clear drag-and-drop data */
    gui.row_drop_path = NULL;
    gui.row_drop_spot = NONE;
  }

  /* Progress Bar */
  {
    gui.progress_bar = gtk_progress_bar_new ();
    gtk_box_pack_end (GTK_BOX (window_vbox_minor), gui.progress_bar, FALSE, FALSE, 0);
  }

  gtk_widget_show_all (gui.window);

  /* Try to load command line specified project if any */
  /* but if any setting files failed to load, bail out */
  if (fatal_message)
  {
    generic_fatal (&gui, fatal_message);
  }
  else if (filename)
  {
    gcode_init (&gui.gcode);
    gui_attach (&gui.gcode, &gui);

    gui.gcode.format = GCODE_FORMAT_BIN;

    if (gcode_load (&gui.gcode, filename) != 0)
    {
      gui.gcode.format = GCODE_FORMAT_XML;

      if (gcode_load (&gui.gcode, filename) != 0)
      {
        gui.gcode.format = GCODE_FORMAT_TBD;
      }
    }

    if (gui.gcode.format == GCODE_FORMAT_TBD)
    {
      generic_error (&gui, "\nSomething went wrong - failed to load the specified file\n");
    }
    else
    {
      strcpy (gui.filename, filename);
      strcpy (gui.current_folder, dirname (filename));

      gcode_prep (&gui.gcode);

      gui_show_project (&gui);

      update_project_modified_flag (&gui, 0);
    }
  }

  gtk_main ();
}
