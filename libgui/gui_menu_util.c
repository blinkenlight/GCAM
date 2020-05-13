/**
 *  gui_menu_util.c
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

#include "gui_menu_util.h"
#include "gui_define.h"
#include "gui_tab.h"
#include <time.h>

#define GUI_TREE_ROOT NULL

/* NOT AN EXACT CONVERSION - uses x25 for round values */
#define SCALED_INCHES(x) (GCODE_UNITS ((&gui->gcode), x))

/**
 * Update the values, ranges and increments of a series of GUI elements in the 
 * "new project" or "update project" dialogs when units are selected (inch/mm)
 * WARNING: any change in the structure of "wlist" affecting the spin buttons
 * in either of the two dialogs mentioned will obviously break this;
 */

void
base_unit_changed_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  GtkSpinButton *spinner;
  gfloat_t value;
  gfloat_t min, max;
  gfloat_t step, page;
  gfloat_t scale_factor;
  gfloat_t max_clr_z;
  char *text_field;
  int unit_in_use;
  int chosen_unit;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  scale_factor = 1.0;

  gtk_spin_button_get_range (GTK_SPIN_BUTTON (wlist[10]), NULL, &max_clr_z);

  if (fabs(max_clr_z) < fabs(MAX_CLR_Z * 10))                                   // Comparing a value that gets rounded for metric units
    unit_in_use = GCODE_UNITS_INCH;                                             // too closely to its original value would be a mistake;
  else                                                                          // All we need to decide is what 'max_clr_z' is close to,
    unit_in_use = GCODE_UNITS_MILLIMETER;                                       // imperial 'MAX_CLR_Z' or metric '25.4 x MAX_CLR_Z'...

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));

  if (strstr (text_field, "inch"))
    chosen_unit = GCODE_UNITS_INCH;
  else
    chosen_unit = GCODE_UNITS_MILLIMETER;

  g_free (text_field);

  if ((unit_in_use == GCODE_UNITS_INCH) && (chosen_unit == GCODE_UNITS_MILLIMETER))
    scale_factor = GCODE_INCH2MM;

  if ((unit_in_use == GCODE_UNITS_MILLIMETER) && (chosen_unit == GCODE_UNITS_INCH))
    scale_factor = GCODE_MM2INCH;

  if (scale_factor == 1.0)
    return;

  for (int i = 4; i <= 10; i++)
  {
    spinner = GTK_SPIN_BUTTON (wlist[i]);

    value = gtk_spin_button_get_value (spinner);

    gtk_spin_button_get_range (spinner, &min, &max);
    gtk_spin_button_set_range (spinner, min * scale_factor, max * scale_factor);

    gtk_spin_button_get_increments (spinner, &step, &page);
    gtk_spin_button_set_increments (spinner, step * scale_factor, page * scale_factor);

    gtk_spin_button_set_value (spinner, value * scale_factor);
  }
}

/**
 * Update the progress bar on the bottom of the GUI
 */

void
update_progress (void *gui, gfloat_t progress)
{
  static gfloat_t old_value = 0;
  static clock_t old_clock = 0;
  clock_t now_clock;
  uint8_t update;

  update = 0;

  now_clock = clock ();

  if ((progress == 0.0) || (progress == 1.0))                                   // End of scale values are always updated unconditionally;
    update = 1;

  if (progress > old_value)                                                     // Everything else has to be larger than current: "regressing" not allowed;
  {
    if (progress - old_value > 0.01)                                            // If progress is "large enough", show it even if the update is too early;
      update = 1;

    if (now_clock - old_clock > CLOCKS_PER_SEC / 25)                            // Smaller progress gets shown not more often than 25 times a second:
      update = 1;                                                               // thousands of tiny updates would really slow things down otherwise;
  }

  if (update)
  {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (((gui_t *)gui)->progress_bar), progress);

    while (gtk_events_pending ())                                               // One iteration (or even several more) are not enough for reliable updates;
      gtk_main_iteration ();                                                    // By flushing the queue, updates are shown even if we're hammering the CPU.

    old_clock = now_clock;
    old_value = progress;
  }
}

/**
 * Display a generic 'info' styled message box
 */

void
generic_dialog (void *gui, char *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (((gui_t *)gui)->window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   "%s",
                                   message);

  gtk_window_set_title (GTK_WINDOW (dialog), "Info");

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (((gui_t *)gui)->window));
  g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

  gtk_widget_show (dialog);
}

/**
 * Display a generic 'error' styled message box 
 */

void
generic_error (void *gui, char *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (((gui_t *)gui)->window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK,
                                   "%s",
                                   message);

  gtk_window_set_title (GTK_WINDOW (dialog), "Error");

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (((gui_t *)gui)->window));
  g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

  gtk_widget_show (dialog);
}

/**
 * Display a generic 'error' styled message box then quit
 */

void
generic_fatal (void *gui, char *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (((gui_t *)gui)->window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK,
                                   "%s",
                                   message);

  gtk_window_set_title (GTK_WINDOW (dialog), "Fatal Error");

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (((gui_t *)gui)->window));
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (dialog);
}

/**
 * Roto-translate 'block' into tangential continuity with its previous peer
 */

static void
set_tangent_to_previous (gcode_block_t *block)
{
  gcode_vec2d_t p0, p1, p;
  gcode_vec2d_t t0, t1, t;
  gcode_vec2d_t delta;
  gfloat_t angle;

  if (block->prev)
  {
    block->ends (block, p0, p, GCODE_GET);
    block->ends (block, t0, t, GCODE_GET_TANGENT);

    block->prev->ends (block->prev, p, p1, GCODE_GET);
    block->prev->ends (block->prev, t, t1, GCODE_GET_TANGENT);

    GCODE_MATH_VEC2D_SUB (delta, p1, p0);

    angle = GCODE_RAD2DEG * (atan2 (t1[1], t1[0]) - atan2 (t0[1], t0[0]));

    GCODE_MATH_WRAP_TO_360_DEGREES (angle);

    block->spin (block, p0, angle);
    block->move (block, delta);
  }
}

/**
 * This is the TreeView equivalent of 'gtk_ctree_node_is_visible' that GTK+ did
 * not see fit to include, apparently to much confusion of those who needed it;
 */

static int
tree_path_is_visible (GtkTreeView *tree_view, GtkTreePath *path)
{
  GdkRectangle view_rect, cell_rect;

  gtk_tree_view_get_visible_rect (tree_view, &view_rect);
  gtk_tree_view_get_cell_area (tree_view, path, NULL, &cell_rect);

  if ((cell_rect.y >= 0) && (cell_rect.y + cell_rect.height <= view_rect.height))
    return (1);

  return (0);
}

/**
 * Insert 'block' into both the block tree after/under 'selected_block' and the
 * GUI GTK tree after/under 'iter', depending on 'insert_spot' - which, notably,
 * can combine BOTH 'after or under', which will try to insert 'after' then (if
 * that fails) will retry to insert 'under' the specified target. Validity of a
 * target spot is checked via the global validity matrix and/or top level table;
 * The newly created iter is returned, with the row opened and ready for a note.
 * NOTE: there's an unsolvable conceptual disconnect between the iter tree and 
 * the block tree, introduced by the way extrusions are handled separately from
 * the list of children with blocks, but as part of the list - first child - in
 * the iter tree; because of this, 'inserting as a first child' means different
 * things for blocks and iters, and as an attempt to reconcile the two different
 * approaches 'insert_primitive' attempts to 'skip over' the first child when it
 * is of extrusion type and insertion to the beginning of the list is requested.
 */

GtkTreeIter
insert_primitive (gui_t *gui, gcode_block_t *block, gcode_block_t *target_block, GtkTreeIter *target_iter, int insert_spot)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreePath *path;
  GtkTreeIter parent_iter, new_iter;

  if (!block)                                                                   // If 'block' is NULL, abort;
    return new_iter;

  if (!target_block)                                                            // If 'target_block' is null, abort - a selected target must already exist;
    return new_iter;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);                             // Retrieve a reference to the tree model - we'll need it later;

  if (insert_spot & GUI_INSERT_AFTER)                                           // If the request contains "insert after", find the would-be parent;
  {
    if (target_block->parent)                                                   // If 'target_block->parent' exists, check validity of 'block' as its child;
    {
      if (GCODE_IS_VALID_PARENT_CHILD[target_block->parent->type][block->type])
      {
        gcode_insert_after_block (target_block, block);                         // If the operation is valid, insert 'block' after 'target_block',
        new_iter = gui_insert_after_iter (gui, target_iter, block);             // then insert a new iter (new row) based on 'block' after 'iter';
        insert_spot &= ~GUI_ADD_AS_CHILD;                                       // Cancel any requests to retry adding as a child of the target;
      }
      else
      {
        insert_spot &= ~GUI_INSERT_AFTER;                                       // If the operation is not valid, remove it from the request;
      }
    }
    else                                                                        // If 'target_block' has no parent, check validity of 'block' as top level;
    {
      if (GCODE_IS_VALID_IF_NO_PARENT[block->type])
      {
        gcode_insert_after_block (target_block, block);                         // If the operation is valid, insert 'block' after 'target_block',
        new_iter = gui_insert_after_iter (gui, target_iter, block);             // then insert a new iter (new row) based on 'block' after 'iter';
        insert_spot &= ~GUI_ADD_AS_CHILD;                                       // Cancel any requests to retry adding as a child of the target;
      }
      else
      {
        insert_spot &= ~GUI_INSERT_AFTER;                                       // If the operation is not valid, remove it from the request;
      }
    }
  }

  if (insert_spot & GUI_INSERT_UNDER)                                           // If the request (still) contains "insert under", parent would be 'target';
  {
    if (GCODE_IS_VALID_PARENT_CHILD[target_block->type][block->type])           // We do know 'target_block' exists, check validity of 'block' as its child;
    {
      gcode_insert_as_listhead (target_block, block);                           // If it is valid, insert 'block' under 'target_block' as the first child,

      if (target_block->extruder)                                               // Curve ball: if the first child is an extrusion, we need to skip it;
      {
        GtkTreeIter child_iter;

        gtk_tree_model_iter_children (tree_model, &child_iter, target_iter);    // Get an iter to that extrusion as the first child of 'target_iter',

        new_iter = gui_insert_after_iter (gui, &child_iter, block);             // then insert a new iter (new row) based on 'block' AFTER that iter;
      }
      else                                                                      // If there are no extrusions involved (meaning we can proceed normally),
      {
        new_iter = gui_insert_under_iter (gui, target_iter, block);             // then insert a new iter (new row) based on 'block' under 'target_iter';
      }

      insert_spot &= ~GUI_APPEND_UNDER;                                         // Cancel any requests to retry adding as a child of the target;
    }
    else
    {
      insert_spot &= ~GUI_INSERT_UNDER;                                         // If the operation is not valid, remove it from the request;
    }
  }

  if (insert_spot & GUI_APPEND_UNDER)                                           // If the request (still) contains "append under", parent would be 'target';
  {
    if (GCODE_IS_VALID_PARENT_CHILD[target_block->type][block->type])           // We do know 'target_block' exists, check validity of 'block' as its child;
    {
      gcode_append_as_listtail (target_block, block);                           // If it is valid, insert 'block' under 'target_block' as the last child,
      new_iter = gui_append_under_iter (gui, target_iter, block);               // then insert a new iter (new row) based on 'block' under 'target_iter';
    }
    else
    {
      insert_spot &= ~GUI_APPEND_UNDER;                                         // If the operation is not valid, remove it from the request;
    }
  }

  if (insert_spot & GUI_ADD_AFTER_OR_UNDER)                                     // If the request still contains "after" or "under", one of them succeeded,
  {                                                                             // so perform any post-insertion actions;
    if (insert_spot & GUI_INSERT_WITH_TANGENCY)
      set_tangent_to_previous (block);

    if (gtk_tree_model_iter_parent (tree_model, &parent_iter, &new_iter))
    {
      path = gtk_tree_model_get_path (tree_model, &parent_iter);

      gtk_tree_view_expand_to_path (tree_view, path);

      gtk_tree_path_free (path);
    }

    gui_renumber_whole_tree (gui);

    set_selected_row_with_iter (gui, &new_iter);

    update_project_modified_flag (gui, 1);
  }

  return new_iter;
}

/**
 * Remove 'block' from the block tree (but don't free!) then also remove 'iter'
 * from the GUI GTK tree;
 */

void
remove_primitive (gui_t *gui, gcode_block_t *block, GtkTreeIter *iter)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  gcode_splice_list_around (block);
  gtk_tree_store_remove (GTK_TREE_STORE (tree_model), iter);
}

/**
 * Remove 'block' from the block tree & recursively free then also remove 'iter'
 * from the GUI GTK tree;
 */

void
remove_and_destroy_primitive (gui_t *gui, gcode_block_t *block, GtkTreeIter *iter)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  gcode_remove_and_destroy (block);
  gtk_tree_store_remove (GTK_TREE_STORE (tree_model), iter);
}

/**
 * Create new rows corresponding to 'block', its extruder and all its children 
 * and insert them into the tree store before the current first child of 'iter';
 * The new iter pointing to the new row corresponding to 'block' is returned.
 * NOTE: the order number field of the new rows is filled in blank ("0") and it
 * needs to be updated - this is partly to avoid multiple implementations of the
 * order numbering algorithm and partly because a treeview that has been touched
 * probably needs renumbering anyway - older rows displaced by newly introduced
 * ones do not magically change their order numbers to fit their new positions.
 * TL;DR: refreshing order numbers after this is the CALLER'S responsibility!
 */

GtkTreeIter
gui_insert_under_iter (gui_t *gui, GtkTreeIter *iter, gcode_block_t *block)
{
  GtkTreeIter new_iter;
  gcode_block_t *index_block;

  if (!block)
    return new_iter;

  gtk_tree_store_prepend (gui->gcode_block_store, &new_iter, iter);

  gtk_tree_store_set (gui->gcode_block_store, &new_iter,
                      0, 0,
                      1, GCODE_TYPE_STRING[block->type],
                      2, block->status,
                      3, block->flags & GCODE_FLAGS_SUPPRESS,
                      4, block->comment,
                      5, block,
                      -1);                                                      // Create and insert a new GUI row, then fill it up with data from 'block';

  index_block = block->extruder;

  if (index_block)
    gui_append_under_iter (gui, &new_iter, index_block);

  index_block = block->listhead;

  if (block->type == GCODE_TYPE_BOLT_HOLES)
    index_block = NULL;

  while (index_block)
  {
    gui_append_under_iter (gui, &new_iter, index_block);

    index_block = index_block->next;
  }

  return new_iter;
}

/**
 * Create new rows corresponding to 'block', its extruder and all its children 
 * and append them to the tree store after the current last child of 'iter';
 * The new iter pointing to the new row corresponding to 'block' is returned.
 * NOTE: the order number field of the new rows is filled in blank ("0") and it
 * needs to be updated - this is partly to avoid multiple implementations of the
 * order numbering algorithm and partly because a treeview that has been touched
 * probably needs renumbering anyway - older rows displaced by newly introduced
 * ones do not magically change their order numbers to fit their new positions.
 * TL;DR: refreshing order numbers after this is the CALLER'S responsibility!
 */

GtkTreeIter
gui_append_under_iter (gui_t *gui, GtkTreeIter *iter, gcode_block_t *block)
{
  GtkTreeIter new_iter;
  gcode_block_t *index_block;

  if (!block)
    return new_iter;

  gtk_tree_store_append (gui->gcode_block_store, &new_iter, iter);

  gtk_tree_store_set (gui->gcode_block_store, &new_iter,
                      0, 0,
                      1, GCODE_TYPE_STRING[block->type],
                      2, block->status,
                      3, block->flags & GCODE_FLAGS_SUPPRESS,
                      4, block->comment,
                      5, block,
                      -1);                                                      // Create and insert a new GUI row, then fill it up with data from 'block';

  index_block = block->extruder;

  if (index_block)
    gui_append_under_iter (gui, &new_iter, index_block);

  index_block = block->listhead;

  if (block->type == GCODE_TYPE_BOLT_HOLES)
    index_block = NULL;

  while (index_block)
  {
    gui_append_under_iter (gui, &new_iter, index_block);

    index_block = index_block->next;
  }

  return new_iter;
}

/**
 * Create new rows corresponding to 'block', its extruder and all its children 
 * and insert them into the tree store right after 'iter' (before the next row);
 * The new iter pointing to the new row corresponding to 'block' is returned.
 * NOTE: the order number field of the new rows is filled in blank ("0") and it
 * needs to be updated - this is partly to avoid multiple implementations of the
 * order numbering algorithm and partly because a treeview that has been touched
 * probably needs renumbering anyway - older rows displaced by newly introduced
 * ones do not magically change their order numbers to fit their new positions.
 * TL;DR: refreshing order numbers after this is the CALLER'S responsibility!
 */

GtkTreeIter
gui_insert_after_iter (gui_t *gui, GtkTreeIter *iter, gcode_block_t *block)
{
  GtkTreeIter new_iter;
  gcode_block_t *index_block;

  if (!block)
    return new_iter;

  gtk_tree_store_insert_after (gui->gcode_block_store, &new_iter, NULL, iter);

  gtk_tree_store_set (gui->gcode_block_store, &new_iter,
                      0, 0,
                      1, GCODE_TYPE_STRING[block->type],
                      2, block->status,
                      3, block->flags & GCODE_FLAGS_SUPPRESS,
                      4, block->comment,
                      5, block,
                      -1);                                                      // Create and insert a new GUI row, then fill it up with data from 'block';

  index_block = block->extruder;

  if (index_block)
    gui_append_under_iter (gui, &new_iter, index_block);

  index_block = block->listhead;

  if (block->type == GCODE_TYPE_BOLT_HOLES)
    index_block = NULL;

  while (index_block)
  {
    gui_append_under_iter (gui, &new_iter, index_block);

    index_block = index_block->next;
  }

  return new_iter;
}

/**
 * Recursively regenerate everything under 'iter' by first destroying all its 
 * children then walking the list of children of the block corresponding to it
 * and recursively appending them back under 'iter' (including its extruder);
 * Order numbers are refreshed (filled in, actually) after the tree is rebuilt.
 * NOTE: this ONLY operates on the treeview, the block tree is left untouched!
 */

void
gui_recreate_subtree_of (gui_t *gui, GtkTreeIter *iter)
{
  gcode_block_t *block, *index_block;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeIter child_iter;
  GValue value = { 0, };

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);                             // Get a reference to the tree model we're supposed to be working on;

  gtk_tree_model_get_value (tree_model, iter, 5, &value);                       // Using 'iter', get a reference to the block it's based on;
  block = (gcode_block_t *)g_value_get_pointer (&value);
  g_value_unset (&value);

  if (!block->listhead && !block->extruder)                                     // If the block has neither extruder nor children there's nothing to recreate;
    return;

  if (gtk_tree_model_iter_children (tree_model, &child_iter, iter))             // Otherwise, get an iter to the first child of 'iter',
    while (gtk_tree_store_remove (GTK_TREE_STORE (tree_model), &child_iter));   // and mercilessly get rid of every single one of them;

  if (block->extruder)                                                          // If the block has an extruder, add it back as the first child of 'iter';
    gui_append_under_iter (gui, iter, block->extruder);

  index_block = block->listhead;                                                // Get a reference to the first child of 'block' (if any),

  while (index_block)                                                           // and keep crawling the list until all of them are added back under 'iter';
  {
    gui_append_under_iter (gui, iter, index_block);

    index_block = index_block->next;
  }

  if (gtk_tree_model_iter_children (tree_model, &child_iter, iter))             // Slight deja-vu here, but get an iter to the first child of 'iter' AGAIN;
    gui_renumber_subtree_of (gui, &child_iter);                                 // Remember, remember the fifth of No... ugh, no - the NEED TO RENUMBER. Right!
}

/**
 * Regenerate the entire GUI GTK tree by first destroying it then walking the
 * gcode's block tree and calling a recursive append for each top level block;
 * Order numbers are refreshed (filled in, actually) after the tree is rebuilt.
 */

void
gui_recreate_whole_tree (gui_t *gui)
{
  gcode_block_t *index_block;

  gtk_tree_store_clear (gui->gcode_block_store);

  index_block = gui->gcode.listhead;

  while (index_block)
  {
    gui_append_under_iter (gui, GUI_TREE_ROOT, index_block);

    index_block = index_block->next;
  }

  gui_renumber_whole_tree (gui);
}

/**
 * Recursively update the first column / 'order number' of each GTK tree row
 * starting with 'iter' and children, ending with 'iter's last same-level peer;
 * Extrusions get '0', anything else gets numbered from '1' in iterating order.
 * NOTE: it might not be obvious that since renumbering starts from 'iter' and
 * not the first child of its parent, it's a bad idea to call this for anything
 * other than a first element of a tree branch or the first element of the tree.
 */

void
gui_renumber_subtree_of (gui_t *gui, GtkTreeIter *iter)
{
  gcode_block_t *block;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeIter child_iter;
  GValue value = { 0, };
  int i = 1;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  do
  {
    gtk_tree_model_get_value (tree_model, iter, 5, &value);
    block = (gcode_block_t *)g_value_get_pointer (&value);
    g_value_unset (&value);

    if (block->type == GCODE_TYPE_EXTRUSION)
    {
      gtk_tree_store_set (gui->gcode_block_store, iter, 0, 0, -1);
    }
    else
    {
      gtk_tree_store_set (gui->gcode_block_store, iter, 0, i, -1);
      i++;
    }

    if (gtk_tree_model_iter_children (tree_model, &child_iter, iter))
      gui_renumber_subtree_of (gui, &child_iter);

  } while (gtk_tree_model_iter_next (tree_model, iter));
}

/**
 * Regenerate the 'order numbering' of the entire GUI GTK tree by calling a
 * recursive 'gui_renumber_subtree_of()' for the first iter of the GTK tree;
 */

void
gui_renumber_whole_tree (gui_t *gui)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeIter iter;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_model_get_iter_first (tree_model, &iter))
    gui_renumber_subtree_of (gui, &iter);
}

/**
 * Recursively crawl the list of 'block' and add any new tools to the tool list
 */

void
gui_collect_endmills_of (gui_t *gui, gcode_block_t *block)
{
  gcode_block_t *index_block;
  gcode_tool_t *tool;

  if (block)
    index_block = block->listhead;
  else
    index_block = gui->gcode.listhead;

  while (index_block)
  {
    if (index_block->type == GCODE_TYPE_TOOL)
    {
      tool = (gcode_tool_t *)index_block->pdata;

      if (!gui_endmills_find (&gui->endmills, tool->label, FALSE))
        gui_endmills_tack (&gui->endmills, tool->number, tool->diameter, gui->gcode.units, tool->label);
    }

    gui_collect_endmills_of (gui, index_block);

    index_block = index_block->next;
  }
}

/**
 * Recursively crawl the GTK tree until the iter associated with 'block' is found
 */

static void
find_tree_row_iter_with_block (gui_t *gui, GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *found_iter, gcode_block_t *block)
{
  gcode_block_t *test_block;
  GtkTreeIter child_iter;
  GValue value = { 0, };

  do
  {
    gtk_tree_model_get_value (model, iter, 5, &value);
    test_block = (gcode_block_t *)g_value_get_pointer (&value);
    g_value_unset (&value);

    if (block == test_block)
    {
      *found_iter = *iter;
      return;
    }

    if (gtk_tree_model_iter_children (model, &child_iter, iter))
      find_tree_row_iter_with_block (gui, model, &child_iter, found_iter, block);

  } while (gtk_tree_model_iter_next (model, iter));
}

/**
 * Find and return the block currently selected in the GUI as both the tree 
 * block 'selected_block' and the GTK tree iterator 'iter';
 */

void
get_selected_block (gui_t *gui, gcode_block_t **block, GtkTreeIter *iter)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeSelection *selection;
  GValue value = { 0, };

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  selection = gtk_tree_view_get_selection (tree_view);

  *block = NULL;

  if (gtk_tree_selection_get_selected (selection, NULL, iter))
  {
    gtk_tree_model_get_value (tree_model, iter, 5, &value);                     // Using the iter, we can fetch the content of the fifth column of that row,
    *block = (gcode_block_t *)g_value_get_pointer (&value);                     // to retrieve the pointer to the associated block;
    g_value_unset (&value);
  }
}

/**
 * Make a specific GTK tree row 'selected' by its associated iter
 */

void
set_selected_row_with_iter (gui_t *gui, GtkTreeIter *iter)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeSelection *selection;
  GtkTreePath *path;
  GValue value = { 0, };
  gcode_block_t *block;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  gtk_tree_model_get_value (tree_model, iter, 5, &value);                       // Using the iter, we can fetch the content of the fifth column of that row,
  block = (gcode_block_t *)g_value_get_pointer (&value);                        // to retrieve the pointer to the associated block;
  g_value_unset (&value);

  path = gtk_tree_model_get_path (tree_model, iter);                            // Get the path corresponding to the iter to be selected;

  if (gtk_tree_path_get_depth (path) > 1)                                       // If the depth is larger than "1", a valid non-null parent path HAS to exist;
  {
    GtkTreePath *parent_path;

    parent_path = gtk_tree_path_copy (path);                                    // Make a copy of the path to be selected: we need the original path later on;

    gtk_tree_path_up (parent_path);                                             // Obtain the parent of the path to be selected (return value broken, ignore);

    gtk_tree_view_expand_to_path (tree_view, parent_path);                      // Expand the parent, since a path can only be selected if it's not collapsed;

    gtk_tree_path_free (parent_path);                                           // Free the parent path once it's no longer needed;
  }

  selection = gtk_tree_view_get_selection (tree_view);                          // Get a reference to the selection and use it to select the desired tree row;

  gtk_tree_selection_select_iter (selection, iter);

  if (!tree_path_is_visible (tree_view, path))                                  // As a convenience, scroll the selected row into view if it was not visible;
  {
    gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);
  }

  update_menu_by_selected_item (gui, block);                                    // Post-selection housekeeping: update the menu according to the new selection

  gui_tab_display (gui, block, 0);                                              // and make sure the left tab displays the properties of the selected item too;

  gtk_tree_path_free (path);                                                    // Free the selection path once it's no longer needed;
}

/**
 * Make a specific GTK tree row 'selected' by its associated block
 */

void
set_selected_row_with_block (gui_t *gui, gcode_block_t *block)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeSelection *selection;
  GtkTreeIter iter, found_iter;
  GtkTreePath *path;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  tree_model = gtk_tree_view_get_model (tree_view);

  gtk_tree_model_get_iter_first (tree_model, &iter);

  find_tree_row_iter_with_block (gui, tree_model, &iter, &found_iter, block);   // Look up the iter corresponding to the block to be selected;

  path = gtk_tree_model_get_path (tree_model, &found_iter);                     // Get the path corresponding to the iter to be selected;

  if (gtk_tree_path_get_depth (path) > 1)                                       // If the depth is larger than "1", a valid non-null parent path HAS to exist;
  {
    GtkTreePath *parent_path;

    parent_path = gtk_tree_path_copy (path);                                    // Make a copy of the path to be selected: we need the original path later on;

    gtk_tree_path_up (parent_path);                                             // Obtain the parent of the path to be selected (return value broken, ignore);

    gtk_tree_view_expand_to_path (tree_view, parent_path);                      // Expand the parent, since a path can only be selected if it's not collapsed;

    gtk_tree_path_free (parent_path);                                           // Free the parent path once it's no longer needed;
  }

  selection = gtk_tree_view_get_selection (tree_view);                          // Get a reference to the selection and use it to select the desired tree row;

  gtk_tree_selection_select_iter (selection, &found_iter);

  if (!tree_path_is_visible (tree_view, path))                                  // As a convenience, scroll the selected row into view if it was not visible;
  {
    gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);
  }

  update_menu_by_selected_item (gui, block);                                    // Post-selection housekeeping: update the menu according to the new selection

  gui_tab_display (gui, block, 0);                                              // and make sure the left tab displays the properties of the selected item too;

  gtk_tree_path_free (path);                                                    // Free the selection path once it's no longer needed;
}

/**
 * Update the window title by current project name and status (modified or not)
 */

void
update_project_modified_flag (gui_t *gui, uint8_t modified)
{
  gui->modified = modified;

  if (modified)
  {
    if (*gui->gcode.name)
    {
      sprintf (gui->title, "* GCAM Special Edition v%s - %s", VERSION, gui->gcode.name);
    }
    else
    {
      sprintf (gui->title, "* GCAM Special Edition v%s", VERSION);
    }
  }
  else
  {
    if (*gui->gcode.name)
    {
      sprintf (gui->title, "GCAM Special Edition v%s - %s", VERSION, gui->gcode.name);
    }
    else
    {
      sprintf (gui->title, "GCAM Special Edition v%s", VERSION);
    }
  }

  gtk_window_set_title (GTK_WINDOW (gui->window), gui->title);

  if (gui->project_state == PROJECT_OPEN)                                       // Unrelated, but it has to go somewhere and 'modified' is a coinciding event;
  {
    if (*gui->filename)                                                         // The idea is that if there is a filename, the "Save" menu can be enabled;
    {
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Save"), 1);
    }
  }
}

/**
 * Enable/disable various GUI menu items based on project state (open/closed)
 * NOTE: always-enabled and strictly project-state-dependent widgets are fully
 * handled; selection-dependent ones are only disabled whenever the project is 
 * closed, further handling being left to the selection-based menu update;
 */

void
update_menu_by_project_state (gui_t *gui, uint8_t state)
{
  uint8_t open;

  gui->project_state = state;

  open = (state == PROJECT_OPEN) ? TRUE : FALSE;

  /* Widgets to enable always */
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Quit"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/AssistantMenu"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/RenderMenu"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/HelpMenu"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/HelpMenu/Manual"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/HelpMenu/About"), 1);

  /* Widgets to enable when project is closed, disable when project is open */
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/New"), !open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Open"), !open);

  /* Widgets to disable when project is closed */
  if (!open)
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Save"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Remove"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Duplicate"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Translate"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Rotate"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Mirror"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Scale"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Previous"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Next"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Previous"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Next"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Flip Direction"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Optimize Order"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Generate Pattern"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Tool Change"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Template"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Sketch"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Arc"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Line"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Bolt Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Drill Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Point"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Image"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/AssistantMenu/Polygon"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Perspective"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Orthographic"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Iso"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Top"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Left"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Right"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Front"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Back"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/RenderMenu/FinalPart"), 0);
  }

  /* Widgets to enable when project is open, disable when project is closed */
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Save As"), open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Close"), open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import GCAM"), open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import Gerber (RS274X)"), open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import Excellon Drill Holes"), open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import SVG Paths"), open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Export"), open);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Project Settings"), open);
}

/**
 * Enable/disable various GUI menu items based on the currently selected block;
 * NOTE: being selection-based, actions taken here assume the project is open.
 */

void
update_menu_by_selected_item (gui_t *gui, gcode_block_t *selected_block)
{
  /* 
   * This check is here in case a block is deleted right after a new block is
   * inserted/duplicated and the comment field is highlighted.
   */
  if (!selected_block)
    return;

  /**
   * Toggle menu items that should reflect actions available to the current selected block.
   */
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import GCAM"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import Gerber (RS274X)"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import Excellon Drill Holes"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import SVG Paths"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Remove"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Duplicate"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Translate"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Rotate"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Mirror"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Scale"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Previous"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Next"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Previous"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Next"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Flip Direction"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Optimize Order"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Generate Pattern"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Tool Change"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Template"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Sketch"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Arc"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Line"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Bolt Holes"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Drill Holes"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Point"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Image"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/AssistantMenu/Polygon"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Perspective"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Orthographic"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Iso"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Top"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Left"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Right"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Front"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Back"), 1);
  gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/RenderMenu/FinalPart"), 1);

  /* FILLETING */
  if (selected_block->type == GCODE_TYPE_LINE)
  {
    gcode_block_t *connected;

    connected = gcode_sketch_prev_connected (selected_block);

    if (connected)
    {
      if (connected->type != GCODE_TYPE_LINE)
      {
        gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Previous"), 0);
      }
    }
    else
    {
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Previous"), 0);
    }

    connected = gcode_sketch_next_connected (selected_block);

    if (connected)
    {
      if (connected->type != GCODE_TYPE_LINE)
      {
        gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Next"), 0);
      }
    }
    else
    {
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Next"), 0);
    }
  }
  else
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Previous"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Fillet Next"), 0);
  }

  /* DRILL POINTS AND POINT */
  if ((selected_block->type != GCODE_TYPE_DRILL_HOLES) &&
      (selected_block->type != GCODE_TYPE_POINT))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Point"), 0);
  }

  if (selected_block->type == GCODE_TYPE_POINT)
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Tool Change"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Template"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Sketch"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Arc"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Line"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Bolt Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Drill Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Image"), 0);
  }

  /* EXTRUSION */
  if (selected_block->parent)
    if (selected_block->parent->type == GCODE_TYPE_EXTRUSION)
    {
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Perspective"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Orthographic"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Iso"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Top"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Left"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Right"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Front"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Back"), 0);
      gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/RenderMenu/FinalPart"), 0);
    }

  if (selected_block->type == GCODE_TYPE_EXTRUSION)
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Perspective"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Orthographic"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Iso"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Top"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Left"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Right"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Front"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/ViewMenu/Back"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/RenderMenu/FinalPart"), 0);
  }

  /* EDIT MENU */
  if (selected_block->flags & GCODE_FLAGS_LOCK)
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Remove"), 0);
  }

  if (selected_block->type != GCODE_TYPE_SKETCH)
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Optimize Order"), 0);
  }

  if ((selected_block->type != GCODE_TYPE_SKETCH) &&
      (selected_block->type != GCODE_TYPE_DRILL_HOLES))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Generate Pattern"), 0);
  }

  if ((selected_block->type != GCODE_TYPE_SKETCH) &&
      (selected_block->type != GCODE_TYPE_ARC) &&
      (selected_block->type != GCODE_TYPE_LINE))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Flip Direction"), 0);
  }

  /* LINE AND ARC */
  if ((selected_block->type == GCODE_TYPE_BEGIN) ||
      (selected_block->type == GCODE_TYPE_END) ||
      (selected_block->type == GCODE_TYPE_TOOL) ||
      (selected_block->type == GCODE_TYPE_TEMPLATE) ||
      (selected_block->type == GCODE_TYPE_BOLT_HOLES) ||
      (selected_block->type == GCODE_TYPE_DRILL_HOLES) ||
      (selected_block->type == GCODE_TYPE_IMAGE))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Line"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Arc"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Previous"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Next"), 0);
  }

  /* ATTRACT NEXT and ATTRACT PREVIOUS */
  if ((selected_block->type == GCODE_TYPE_SKETCH) ||
      (selected_block->type == GCODE_TYPE_EXTRUSION))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Previous"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Attract Next"), 0);
  }

  /* DUPLICATE */
  if ((selected_block->type == GCODE_TYPE_BEGIN) ||
      (selected_block->type == GCODE_TYPE_END) ||
      (selected_block->type == GCODE_TYPE_EXTRUSION))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Duplicate"), 0);
  }

  /* TRANSLATE, ROTATE AND MIRROR */
  if ((selected_block->type != GCODE_TYPE_TEMPLATE) &&
      (selected_block->type != GCODE_TYPE_SKETCH) &&
      (selected_block->type != GCODE_TYPE_BOLT_HOLES) &&
      (selected_block->type != GCODE_TYPE_DRILL_HOLES) &&
      (selected_block->type != GCODE_TYPE_POINT) &&
      (selected_block->type != GCODE_TYPE_LINE) &&
      (selected_block->type != GCODE_TYPE_ARC) &&
      (selected_block->type != GCODE_TYPE_STL))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Translate"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Rotate"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Mirror"), 0);
  }

  /* SCALE */
  if ((selected_block->type != GCODE_TYPE_TEMPLATE) &&
      (selected_block->type != GCODE_TYPE_SKETCH) &&
      (selected_block->type != GCODE_TYPE_BOLT_HOLES) &&
      (selected_block->type != GCODE_TYPE_DRILL_HOLES) &&
      (selected_block->type != GCODE_TYPE_LINE) &&
      (selected_block->type != GCODE_TYPE_ARC) &&
      (selected_block->type != GCODE_TYPE_IMAGE) &&
      (selected_block->type != GCODE_TYPE_STL))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/EditMenu/Scale"), 0);
  }

  /* POLYGON ASSISTANT */
  if ((selected_block->type != GCODE_TYPE_TOOL) &&
      (selected_block->type != GCODE_TYPE_TEMPLATE) &&
      (selected_block->type != GCODE_TYPE_SKETCH))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/AssistantMenu/Polygon"), 0);
  }

  /* End - Nothing allowed to be inserted or pasted */
  if (selected_block->type == GCODE_TYPE_END)
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import GCAM"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import Gerber (RS274X)"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import Excellon Drill Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/FileMenu/Import SVG Paths"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Tool Change"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Sketch"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Bolt Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Template"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Arc"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Line"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Drill Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Image"), 0);
  }

  if ((selected_block->type == GCODE_TYPE_LINE) ||
      (selected_block->type == GCODE_TYPE_ARC) ||
      (selected_block->type == GCODE_TYPE_EXTRUSION))
  {
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Tool Change"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Sketch"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Bolt Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Template"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Drill Holes"), 0);
    gtk_action_set_sensitive (gtk_ui_manager_get_action (gui->ui_manager, "/MainMenu/InsertMenu/Image"), 0);
  }
}

/**
 * Recreate the main tree view based on the underlying block tree, select the
 * "tool" row, declare the project state "open", update the left tab and also 
 * enable / disable menu items accordingly, then repaint the rendering;
 * Once a new project is created or loaded, this is the routine to call.
 */

void
gui_show_project (gui_t *gui)
{
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;
  GtkTreePath *path;
  GtkTreeIter selected_iter;
  gcode_block_t *selected_block;

  tree_view = GTK_TREE_VIEW (gui->gcode_block_treeview);

  /* Refresh G-Code Block Tree */
  gui_recreate_whole_tree (gui);

  /* Highlight the Tool block */
  path = gtk_tree_path_new_from_string ("1");
  selection = gtk_tree_view_get_selection (tree_view);

  gtk_tree_selection_select_path (selection, path);
  get_selected_block (gui, &selected_block, &selected_iter);
  gui_tab_display (gui, selected_block, 0);

  gtk_tree_path_free (path);

  update_menu_by_project_state (gui, PROJECT_OPEN);
  update_menu_by_selected_item (gui, selected_block);

  gui->opengl.ready = 1;
  gui->first_render = 1;

  gui_opengl_context_prep (&gui->opengl);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

/**
 * Full-on Dalek mode: Engage! Burn the land, boil the sea and all that jazz...
 */

void
gui_destroy (void)
{
  gtk_main_quit ();
}
