/**
 *  gui_menu_file.c
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

#include "gui_menu_file.h"
#include "gui_menu_util.h"
#include "gui.h"
#include "gui_define.h"
#include "gui_tab.h"
#include <libgen.h>

/* NOT AN EXACT CONVERSION - uses x25 for round values */
#define DEFVAL_INCHES(x) (EQUIV_UNITS ((DEF_UNITS), x))
#define SCALED_INCHES(x) (GCODE_UNITS ((&gui->gcode), x))

#define GCAM_CLOSE_LINE1 "<b>Save changes to project \"%s\" before closing?</b>"
#define GCAM_CLOSE_LINE2 "If you close without saving, your changes will be discarded."
#define GCAM_STOCK_CLOSE "Close _without Saving"

/**
 * Destroy the "new project" assistant on "cancel" or "close" and free "wlist"
 */

static void
new_project_on_assistant_close_cancel (GtkWidget *assistant, gpointer data)
{
  GtkWidget **wlist;

  wlist = (GtkWidget **)data;

  gtk_widget_destroy (assistant);

  free (wlist);
}

/**
 * Initialize cleanly the 'gcode' object of 'gui' and set it up according to the
 * data collected by the new project dialog, populate it with a 'begin', 'tool'
 * and 'end' block then update everything in the GUI to reflect the new project;
 * NOTE: This is a callback for the "Create" event of the new project dialog
 */

static void
new_project_on_assistant_apply (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  gcode_block_t *block;
  gui_machine_t *machine;
  gui_endmill_t *endmill;
  gfloat_t tool_diameter;
  int chosen_unit, material_type, tool_number;
  char *text_field, tool_name[32], project_name[32];

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  gcode_init (&gui->gcode);
  gui_attach (&gui->gcode, gui);

  strcpy (project_name, gtk_entry_get_text (GTK_ENTRY (wlist[1])));

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));

  if (strstr (text_field, "inch"))
    chosen_unit = GCODE_UNITS_INCH;
  else
    chosen_unit = GCODE_UNITS_MILLIMETER;

  g_free (text_field);

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[3]));

  if (strstr (text_field, "aluminium"))
  {
    material_type = GCODE_MATERIAL_ALUMINUM;
  }
  else if (strstr (text_field, "foam"))
  {
    material_type = GCODE_MATERIAL_FOAM;
  }
  else if (strstr (text_field, "plastic"))
  {
    material_type = GCODE_MATERIAL_PLASTIC;
  }
  else if (strstr (text_field, "steel"))
  {
    material_type = GCODE_MATERIAL_STEEL;
  }
  else if (strstr (text_field, "wood"))
  {
    material_type = GCODE_MATERIAL_WOOD;
  }
  else
  {
    /* Set default to steel since it's the safest choice */
    material_type = GCODE_MATERIAL_STEEL;
  }

  g_free (text_field);

  strcpy (gui->gcode.name, project_name);

  gui->gcode.units = chosen_unit;

  gui->gcode.material_type = material_type;

  gui->gcode.material_size[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[4]));
  gui->gcode.material_size[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));
  gui->gcode.material_size[2] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));

  gui->gcode.material_origin[0] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[7]));
  gui->gcode.material_origin[1] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[8]));
  gui->gcode.material_origin[2] = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[9]));

  gui->gcode.ztraverse = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[10]));

  /* Machine Name */
  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[11]));

  machine = gui_machines_find (&gui->machines, text_field, TRUE);

  strcpy (gui->gcode.machine_name, machine->name);
  gui->gcode.machine_options = machine->options;

  g_free (text_field);

  /* Endmill Label */
  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[12]));
  strcpy (tool_name, &text_field[6]);
  g_free (text_field);

  endmill = gui_endmills_find (&gui->endmills, tool_name, TRUE);

  tool_diameter = gui_endmills_size (endmill, gui->gcode.units);
  tool_number = endmill->number;

  /**
   * Setup G-Code Block List and Populate with a Begin, Initial Tool, and End.
   */

  gcode_begin_init (&block, &gui->gcode, NULL);

  block->make (block);

  gcode_append_as_listtail (NULL, block);                                       // At this point, the list of 'gcode' has to be NULL; 'block' becomes listhead.

  {
    gcode_tool_t *tool;

    gcode_tool_init (&block, &gui->gcode, NULL);

    tool = (gcode_tool_t *)block->pdata;

    tool->diameter = tool_diameter;
    tool->number = tool_number;
    strcpy (tool->label, tool_name);

    block->make (block);
  }

  gcode_append_as_listtail (NULL, block);                                       // Appending 'block' after the new listhead;

  gcode_end_init (&block, &gui->gcode, NULL);

  block->make (block);

  gcode_append_as_listtail (NULL, block);                                       // Appending 'block' as the final element in the list.

  gcode_prep (&gui->gcode);

  gui_show_project (gui);

  update_project_modified_flag (gui, 1);
}

/**
 * Populate the "new project" assistant with all the controls required to set
 * up a new project, then hook up a callback for the "unit" selection combo;
 * WARNING: Any change in the structure of "wlist" affecting the spin buttons 
 * will break "base_unit_changed_callback" unless it gets updated accordingly
 */

static void
new_project_create_page1 (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *name_entry;
  GtkWidget *base_unit_combo;
  GtkWidget *material_type_combo;
  GtkWidget *material_sizex_spin;
  GtkWidget *material_sizey_spin;
  GtkWidget *material_sizez_spin;
  GtkWidget *material_originx_spin;
  GtkWidget *material_originy_spin;
  GtkWidget *material_originz_spin;
  GtkWidget *end_mill_combo;
  GtkWidget *machine_combo;
  GtkWidget *ztraverse_spin;
  GtkWidget **wlist;
  GdkPixbuf *pixbuf;

  int active_unit = DEF_UNITS;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  table = gtk_table_new (8, 4, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), BORDER_WIDTH);

  label = gtk_label_new ("Project Name");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

  name_entry = gtk_entry_new ();
  gtk_entry_set_max_length (GTK_ENTRY (name_entry), 32);
  gtk_entry_set_text (GTK_ENTRY (name_entry), "Part");
  gtk_entry_set_activates_default (GTK_ENTRY (name_entry), TRUE);
  gtk_table_attach (GTK_TABLE (table), name_entry, 1, 4, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  label = gtk_label_new ("Base Unit");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

  base_unit_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (base_unit_combo), "inch");
  gtk_combo_box_append_text (GTK_COMBO_BOX (base_unit_combo), "millimeter");

  if (active_unit == GCODE_UNITS_INCH)
    gtk_combo_box_set_active (GTK_COMBO_BOX (base_unit_combo), 0);
  else
    gtk_combo_box_set_active (GTK_COMBO_BOX (base_unit_combo), 1);

  gtk_table_attach (GTK_TABLE (table), base_unit_combo, 1, 4, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  g_signal_connect (base_unit_combo, "changed", G_CALLBACK (base_unit_changed_callback), wlist);

  label = gtk_label_new ("Material Type");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);

  material_type_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (material_type_combo), "aluminium");
  gtk_combo_box_append_text (GTK_COMBO_BOX (material_type_combo), "foam");
  gtk_combo_box_append_text (GTK_COMBO_BOX (material_type_combo), "plastic");
  gtk_combo_box_append_text (GTK_COMBO_BOX (material_type_combo), "steel");
  gtk_combo_box_append_text (GTK_COMBO_BOX (material_type_combo), "wood");
  gtk_combo_box_set_active (GTK_COMBO_BOX (material_type_combo), 0);
  gtk_table_attach (GTK_TABLE (table), material_type_combo, 1, 4, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  label = gtk_label_new ("Material Size (X/Y/Z)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);

  material_sizex_spin = gtk_spin_button_new_with_range (DEFVAL_INCHES (0.01), DEFVAL_INCHES (MAX_DIM_X), DEFVAL_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (material_sizex_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (material_sizex_spin), DEFVAL_INCHES (3.0));
  gtk_table_attach_defaults (GTK_TABLE (table), material_sizex_spin, 1, 2, 3, 4);

  g_signal_connect_swapped (material_sizex_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  material_sizey_spin = gtk_spin_button_new_with_range (DEFVAL_INCHES (0.01), DEFVAL_INCHES (MAX_DIM_Y), DEFVAL_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (material_sizey_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (material_sizey_spin), DEFVAL_INCHES (2.0));
  gtk_table_attach_defaults (GTK_TABLE (table), material_sizey_spin, 2, 3, 3, 4);

  g_signal_connect_swapped (material_sizey_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  material_sizez_spin = gtk_spin_button_new_with_range (DEFVAL_INCHES (0.01), DEFVAL_INCHES (MAX_DIM_Z), DEFVAL_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (material_sizez_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (material_sizez_spin), DEFVAL_INCHES (0.25));
  gtk_table_attach_defaults (GTK_TABLE (table), material_sizez_spin, 3, 4, 3, 4);

  g_signal_connect_swapped (material_sizez_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  label = gtk_label_new ("Material Origin (X/Y/Z)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 4, 5);

  material_originx_spin = gtk_spin_button_new_with_range (DEFVAL_INCHES (0.0), DEFVAL_INCHES (MAX_DIM_X), DEFVAL_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (material_originx_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (material_originx_spin), DEFVAL_INCHES (0.0));
  gtk_table_attach_defaults (GTK_TABLE (table), material_originx_spin, 1, 2, 4, 5);

  g_signal_connect_swapped (material_originx_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  material_originy_spin = gtk_spin_button_new_with_range (DEFVAL_INCHES (0.0), DEFVAL_INCHES (MAX_DIM_Y), DEFVAL_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (material_originy_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (material_originy_spin), DEFVAL_INCHES (0.0));
  gtk_table_attach_defaults (GTK_TABLE (table), material_originy_spin, 2, 3, 4, 5);

  g_signal_connect_swapped (material_originy_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  material_originz_spin = gtk_spin_button_new_with_range (DEFVAL_INCHES (-MAX_DIM_Z), DEFVAL_INCHES (MAX_DIM_Z), DEFVAL_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (material_originz_spin), 3);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (material_originz_spin), DEFVAL_INCHES (0.0));
  gtk_table_attach_defaults (GTK_TABLE (table), material_originz_spin, 3, 4, 4, 5);

  g_signal_connect_swapped (material_originz_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  label = gtk_label_new ("Traverse Height (Z)");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 5, 6);

  ztraverse_spin = gtk_spin_button_new_with_range (DEFVAL_INCHES (0.0), DEFVAL_INCHES (MAX_CLR_Z), DEFVAL_INCHES (0.01));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (ztraverse_spin), 2);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (ztraverse_spin), DEFVAL_INCHES (0.05));
  gtk_table_attach (GTK_TABLE (table), ztraverse_spin, 1, 4, 5, 6, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  g_signal_connect_swapped (ztraverse_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  {
    char string[32];
    int i;

    label = gtk_label_new ("End Mill");
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 6, 7);
    end_mill_combo = gtk_combo_box_new_text ();

    for (i = 0; i < gui->endmills.number; i++)
    {
      if (gui->endmills.endmill[i].origin == GUI_ENDMILL_INTERNAL)
      {
        sprintf (string, "T%.2d - %s", gui->endmills.endmill[i].number, gui->endmills.endmill[i].description);
        gtk_combo_box_append_text (GTK_COMBO_BOX (end_mill_combo), string);
      }
    }

    gtk_combo_box_set_active (GTK_COMBO_BOX (end_mill_combo), 0);
    gtk_table_attach (GTK_TABLE (table), end_mill_combo, 1, 4, 6, 7, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  }

  {
    int i;

    label = gtk_label_new ("Machine");
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 7, 8);
    machine_combo = gtk_combo_box_new_text ();

    for (i = 0; i < gui->machines.number; i++)
      gtk_combo_box_append_text (GTK_COMBO_BOX (machine_combo), gui->machines.machine[i].name);

    gtk_combo_box_set_active (GTK_COMBO_BOX (machine_combo), 0);
    gtk_table_attach (GTK_TABLE (table), machine_combo, 1, 4, 7, 8, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  }

  wlist[1] = name_entry;
  wlist[2] = base_unit_combo;
  wlist[3] = material_type_combo;
  wlist[4] = material_sizex_spin;
  wlist[5] = material_sizey_spin;
  wlist[6] = material_sizez_spin;
  wlist[7] = material_originx_spin;
  wlist[8] = material_originy_spin;
  wlist[9] = material_originz_spin;
  wlist[10] = ztraverse_spin;
  wlist[11] = machine_combo;
  wlist[12] = end_mill_combo;

  gtk_widget_show_all (table);

  gtk_assistant_append_page (GTK_ASSISTANT (assistant), table);
  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), table, "New Project");
  gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), table, GTK_ASSISTANT_PAGE_CONFIRM);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), table, TRUE);

  pixbuf = gtk_widget_render_icon (assistant, GTK_STOCK_NEW, GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_assistant_set_page_header_image (GTK_ASSISTANT (assistant), table, pixbuf);
  g_object_unref (pixbuf);
}

/**
 * Construct an assistant and call another routine that will populate it with
 * controls to configure the various parameters of a new project, then hook up 
 * callbacks for the assistant's "cancel", "close" and "apply" events;
 * NOTE: This is a callback for the "New Project" item of the "File" menu
 */

void
gui_menu_file_new_project_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget *assistant;
  GtkWidget **wlist;

  gui = (gui_t *)data;

  assistant = gtk_assistant_new ();

  gtk_window_set_title (GTK_WINDOW (assistant), "New Project");
  gtk_window_set_default_size (GTK_WINDOW (assistant), -1, -1);
  gtk_window_set_screen (GTK_WINDOW (assistant), gtk_widget_get_screen (gui->window));
  gtk_window_set_transient_for (GTK_WINDOW (assistant), GTK_WINDOW (gui->window));

  /* Setup Global Widgets */
  wlist = malloc (13 * sizeof (GtkWidget *));

  wlist[0] = (void *)gui;

  new_project_create_page1 (assistant, wlist);

  g_signal_connect (assistant, "cancel", G_CALLBACK (new_project_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "close", G_CALLBACK (new_project_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "apply", G_CALLBACK (new_project_on_assistant_apply), wlist);

  gtk_widget_show (assistant);
}

/**
 * Select a GCAM project file to be loaded, attempt loading its contents into 
 * the main gcode as either binary or xml, then update everything accordingly;
 * NOTE: This is a callback for the "Load Project" item of the "File" menu
 */

void
gui_menu_file_load_project_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gui_t *gui;

  gui = (gui_t *)data;

  dialog = gtk_file_chooser_dialog_new ("Open GCAM Project",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM project (*.gcam,*.gcamx)");
  gtk_file_filter_add_pattern (filter, "*.gcam");
  gtk_file_filter_add_pattern (filter, "*.gcamx");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM binary (*.gcam)");
  gtk_file_filter_add_pattern (filter, "*.gcam");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM xml (*.gcamx)");
  gtk_file_filter_add_pattern (filter, "*.gcamx");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All files (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    gcode_init (&gui->gcode);
    gui_attach (&gui->gcode, gui);

    gui->gcode.format = GCODE_FORMAT_BIN;

    if (gcode_load (&gui->gcode, filename) != 0)
    {
      gui->gcode.format = GCODE_FORMAT_XML;

      if (gcode_load (&gui->gcode, filename) != 0)
      {
        gui->gcode.format = GCODE_FORMAT_TBD;
      }
    }

    if (gui->gcode.format == GCODE_FORMAT_TBD)
    {
      generic_error (gui, "\nSomething went wrong - failed to load the file\n");
    }
    else
    {
      strcpy (gui->filename, filename);
      strcpy (gui->current_folder, dirname (filename));

      gcode_prep (&gui->gcode);

      gui_collect_endmills_of (gui, NULL);

      gui_show_project (gui);

      update_project_modified_flag (gui, 0);
    }

    g_free (filename);
  }

  gtk_widget_destroy (dialog);
}

/**
 * Save the current project with the file name currently stored within 'gui';
 * NOTE: This is a callback for the "Save Project" item of the "File" menu
 */

void
gui_menu_file_save_project_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;

  gui = (gui_t *)data;

  gcode_save (&gui->gcode, gui->filename);

  update_project_modified_flag (gui, 0);
}

/**
 * Select a file name to save the current project under then perform the save;
 * NOTE: This is a callback for the "Save Project As" item of the "File" menu
 */

gint
gui_menu_file_save_project_as_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gui_t *gui;
  gint response;
  char proposed_filename[64];

  gui = (gui_t *)data;

  dialog = gtk_file_chooser_dialog_new ("Save GCAM Project As",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

  sprintf (proposed_filename, "%s.gcam", gui->gcode.name);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), proposed_filename);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM project (*.gcam,*.gcamx)");
  gtk_file_filter_add_pattern (filter, "*.gcam");
  gtk_file_filter_add_pattern (filter, "*.gcamx");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM binary (*.gcam)");
  gtk_file_filter_add_pattern (filter, "*.gcam");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM xml (*.gcamx)");
  gtk_file_filter_add_pattern (filter, "*.gcamx");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All files (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    strcpy (gui->filename, filename);
    strcpy (gui->current_folder, dirname (filename));

    gui->gcode.format = GCODE_FORMAT_TBD;                                       // The file format should be determined anew by the file extension

    gcode_save (&gui->gcode, gui->filename);

    update_project_modified_flag (gui, 0);

    g_free (filename);
  }

  gtk_widget_destroy (dialog);

  return (response);
}

/**
 * Depending on the chosen response, do or do not save the current project under
 * either the file name stored within 'gui' or a newly selected one then discard
 * (free) all blocks of the current project and update everything accordingly;
 * NOTE: This is a callback for the "Response" event of the close project dialog
 */

static void
close_callback (GtkDialog *dialog, gint response, gpointer data)
{
  gui_t *gui;
  gint result;

  gui = (gui_t *)data;

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (response == GTK_RESPONSE_CANCEL)
    return;

  if (response == GTK_RESPONSE_DELETE_EVENT)
    return;

  if (response == GTK_RESPONSE_ACCEPT)
  {
    if (*gui->filename)
    {
      gcode_save (&gui->gcode, gui->filename);
    }
    else
    {
      result = gui_menu_file_save_project_as_menuitem_callback (NULL, gui);

      if (result != GTK_RESPONSE_ACCEPT)
        return;
    }
  }

  gcode_free (&gui->gcode);

  /* Eliminate external endmills */
  gui_endmills_cull (&gui->endmills);

  /* Refresh G-Code Block Tree */
  gui_recreate_whole_tree (gui);

  /* Context */
  gui->opengl.ready = 0;
  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, NULL);

  gui_tab_display (gui, NULL, 0);

  strcpy (gui->gcode.name, "");
  strcpy (gui->filename, "");

  update_menu_by_project_state (gui, PROJECT_CLOSED);
  update_project_modified_flag (gui, 0);
}

/**
 * If the project is modified, construct a dialog asking whether or not to save
 * it then hook up a suitable callback for the dialog's "response" event; if the
 * project needs no saving, discard (free) all blocks of the current project and
 * update everything accordingly;
 * NOTE: This is a callback for the "Close Project" item of the "File" menu
 */

void
gui_menu_file_close_project_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  gui_t *gui;

  gui = (gui_t *)data;

  if (gui->project_state == PROJECT_CLOSED)
    return;

  if (!gui->modified)
  {
    gcode_free (&gui->gcode);

    /* Refresh G-Code Block Tree */
    gui_recreate_whole_tree (gui);

    /* Context */
    gui->opengl.ready = 0;
    gui->opengl.rebuild_view_display_list = 1;
    gui_opengl_context_redraw (&gui->opengl, NULL);

    gui_tab_display (gui, NULL, 0);

    strcpy (gui->gcode.name, "");
    strcpy (gui->filename, "");

    update_menu_by_project_state (gui, PROJECT_CLOSED);
    update_project_modified_flag (gui, 0);

    return;
  }

  dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gui->window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_WARNING,
                                               GTK_BUTTONS_NONE,
                                               GCAM_CLOSE_LINE1,
                                               gui->gcode.name);

  gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
                                              GCAM_CLOSE_LINE2);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GCAM_STOCK_CLOSE, GTK_RESPONSE_REJECT);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  if (*gui->filename)
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT);
  else
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gui->window));

  g_signal_connect (dialog, "response", G_CALLBACK (close_callback), gui);

  gtk_widget_show (dialog);
}

/**
 * Destroy the "export g-code" assistant on "cancel"/"close" and free "wlist"
 */

static void
export_gcode_on_assistant_close_cancel (GtkWidget *assistant, gpointer data)
{
  GtkWidget **wlist;

  wlist = (GtkWidget **)data;

  gtk_widget_destroy (assistant);

  free (wlist);
}

/**
 * Select a file to export generated g-code into then perform the actual export;
 * NOTE: This is a callback for the export assistant's "apply" event
 */

static void
export_gcode_on_assistant_apply (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  GtkWidget *dialog;
  GtkFileFilter *filter;
  char proposed_filename[64];
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[1]));

  if (strstr (text_field, "LinuxCNC"))
  {
    gui->gcode.driver = GCODE_DRIVER_LINUXCNC;
  }
  else if (strstr (text_field, "TurboCNC"))
  {
    gui->gcode.driver = GCODE_DRIVER_TURBOCNC;
  }
  else if (strstr (text_field, "Haas"))
  {
    gui->gcode.driver = GCODE_DRIVER_HAAS;
  }

  g_free (text_field);

  gui->gcode.project_number = (uint32_t)gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[2]));

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[3]));

  if (strstr (text_field, "Canned"))
  {
    gui->gcode.drilling_motion = GCODE_DRILLING_CANNED;
  }
  else if (strstr (text_field, "Simple"))
  {
    gui->gcode.drilling_motion = GCODE_DRILLING_SIMPLE;
  }

  g_free (text_field);

  dialog = gtk_file_chooser_dialog_new ("Export G-Code",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

  sprintf (proposed_filename, "%s.nc", gui->gcode.name);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), proposed_filename);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "*.nc");
  gtk_file_filter_add_pattern (filter, "*.nc");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    gcode_export (&gui->gcode, filename);

    g_free (filename);
  }

  gtk_widget_destroy (dialog);
}

/**
 * Enable/disable a widget in the g-code export dialog based on selected format;
 * NOTE: This is a callback for the export assistant's "format changed" event
 */

static void
export_format_changed_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  char *text_field;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[1]));

  if (strstr (text_field, "EMC"))
  {
    gtk_widget_set_sensitive (wlist[2], 0);
  }
  else if (strstr (text_field, "TurboCNC"))
  {
    gtk_widget_set_sensitive (wlist[2], 0);
  }
  else if (strstr (text_field, "Haas"))
  {
    gtk_widget_set_sensitive (wlist[2], 1);
  }

  g_free (text_field);
}

static void
export_gcode_create_page1 (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *export_format_combo;
  GtkWidget *project_number_spin;
  GtkWidget *drilling_motion_combo;
  GtkWidget **wlist;
  GdkPixbuf *pixbuf;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  table = gtk_table_new (3, 2, TRUE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), BORDER_WIDTH);

  label = gtk_label_new ("Format");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

  export_format_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (export_format_combo), "EMC");
  gtk_combo_box_append_text (GTK_COMBO_BOX (export_format_combo), "TurboCNC");
  gtk_combo_box_append_text (GTK_COMBO_BOX (export_format_combo), "Haas");
  gtk_combo_box_set_active (GTK_COMBO_BOX (export_format_combo), 0);
  gtk_table_attach (GTK_TABLE (table), export_format_combo, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  g_signal_connect (export_format_combo, "changed", G_CALLBACK (export_format_changed_callback), wlist);

  label = gtk_label_new ("Project Number");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

  project_number_spin = gtk_spin_button_new_with_range (0.0, 99999.0, 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (project_number_spin), 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (project_number_spin), 100.0);
  gtk_table_attach (GTK_TABLE (table), project_number_spin, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  g_signal_connect_swapped (project_number_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  label = gtk_label_new ("Drill Using");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);

  drilling_motion_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (drilling_motion_combo), "Canned cycles (G8x)");
  gtk_combo_box_append_text (GTK_COMBO_BOX (drilling_motion_combo), "Simple motion (G0x)");
  gtk_combo_box_set_active (GTK_COMBO_BOX (drilling_motion_combo), 0);
  gtk_table_attach (GTK_TABLE (table), drilling_motion_combo, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  if (gui->gcode.drilling_motion == GCODE_DRILLING_CANNED)
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (drilling_motion_combo), 0);
  }
  else if (gui->gcode.drilling_motion == GCODE_DRILLING_SIMPLE)
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (drilling_motion_combo), 1);
  }

  wlist[1] = export_format_combo;
  wlist[2] = project_number_spin;
  wlist[3] = drilling_motion_combo;

  gtk_widget_set_sensitive (project_number_spin, 0);

  gtk_widget_show_all (table);

  gtk_assistant_append_page (GTK_ASSISTANT (assistant), table);
  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), table, "Export G-code");
  gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), table, GTK_ASSISTANT_PAGE_CONFIRM);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), table, TRUE);

  pixbuf = gtk_widget_render_icon (assistant, GTK_STOCK_CONVERT, GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_assistant_set_page_header_image (GTK_ASSISTANT (assistant), table, pixbuf);
  g_object_unref (pixbuf);
}

/**
 * Construct an assistant and call another routine that will populate it with
 * controls to configure g-code export parameters of a new project, then hook
 * up callbacks for the assistant's "cancel", "close" and "apply" events;
 * NOTE: This is a callback for the "Export G-Code" item of the "File" menu
 */

void
gui_menu_file_export_gcode_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget *assistant;
  GtkWidget **wlist;

  gui = (gui_t *)data;

  assistant = gtk_assistant_new ();

  gtk_window_set_title (GTK_WINDOW (assistant), "Export G-code");
  gtk_window_set_default_size (GTK_WINDOW (assistant), -1, -1);
  gtk_window_set_screen (GTK_WINDOW (assistant), gtk_widget_get_screen (gui->window));
  gtk_window_set_transient_for (GTK_WINDOW (assistant), GTK_WINDOW (gui->window));

  /* Setup Global Widgets */
  wlist = malloc (4 * sizeof (GtkWidget *));

  wlist[0] = (void *)gui;

  export_gcode_create_page1 (assistant, wlist);

  g_signal_connect (assistant, "cancel", G_CALLBACK (export_gcode_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "close", G_CALLBACK (export_gcode_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "apply", G_CALLBACK (export_gcode_on_assistant_apply), wlist);

  gtk_widget_show (assistant);
}

/**
 * Destroy or free every dynamic resource related to the import list dialog;
 * NOTE: This is a callback for the "cancel" event of the import list dialog
 */

static void
cancel_import_gcam_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;

  wlist = (GtkWidget **)data;                                                   // Retrieve a reference to the graphic context data;

  gtk_widget_destroy (GTK_WIDGET (wlist[1]));                                   // Destroy the import block list dialog;

  gcode_free ((gcode_t *)wlist[3]);                                             // Free the graphic context;
  free (wlist[3]);
  free (wlist);
}

/**
 * Take the block selected in the import list dialog and import (clone) it with
 * all its children from the temporary gcode object over into the main gcode;
 * NOTE: This is a callback for the "import" event of the import list dialog
 */

static void
import_gcam_block_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget **wlist;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter selected_iter, parent_iter;
  GValue value = { 0, };
  gui_t *gui;
  gcode_block_t *selected_block, *duplicate_block;

  wlist = (GtkWidget **)data;                                                   // Retrieve a reference to the graphic context data;

  gui = (gui_t *)wlist[0];                                                      // From that, retrieve a reference to 'gui';

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (wlist[2]));                   // Retrieve a reference to the model of the import block list tree view;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (wlist[2]));           // Get the current selection on the import block list tree view;
  selected_block = NULL;

  if (gtk_tree_selection_get_selected (selection, NULL, &selected_iter))        // Retrieve an iter to the currently selected item in the import block list;
  {
    gtk_tree_model_get_value (model, &selected_iter, 2, &value);                // Retrieve a reference to the block corresponding to the selected iter
    selected_block = (gcode_block_t *)g_value_get_pointer (&value);             // from the second column of the iter data (which is a 'pointer to block');

    g_value_unset (&value);
  }

  /**
   * This is rather important: if we'd clone the temporarily imported blocks 1:1,
   * the brand new clone blocks would get linked to a parent gcode structure that
   * gets freed as soon as the import is complete - NOT A GOOD IDEA, M'KAY?
   */

  selected_block->clone (&duplicate_block, &gui->gcode, selected_block);        // Create a clone of the selected subtree, with the main gcode as parent (!);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (gui->gcode_block_treeview));  // Retrieve a reference to the model of the main GUI tree view;

  get_selected_block (gui, &selected_block, &selected_iter);                    // Get the currently selected block and iter in the main GUI tree view;

  /* This would not be so elaborate with a more context-aware "File" menu */

  while (selected_block)                                                        // Must back up the tree if the new block is not valid after the selected one;
  {
    if (selected_block->parent)                                                 // Does the selected block have a parent or is it top level?
    {
      if (GCODE_IS_VALID_PARENT_CHILD[selected_block->parent->type][duplicate_block->type])     // If it has one, check validity of the imported block under it;
      {
        insert_primitive (gui, duplicate_block, selected_block, &selected_iter, GUI_INSERT_AFTER);      // If valid, insert it after the selected one and leave;
        break;
      }
    }
    else
    {
      if (GCODE_IS_VALID_IF_NO_PARENT[duplicate_block->type])                   // If no parent, check validity of the imported block as a top level block;
      {
        insert_primitive (gui, duplicate_block, selected_block, &selected_iter, GUI_INSERT_AFTER);      // If valid, insert it after the selected one and leave;
        break;
      }
    }

    gtk_tree_model_iter_parent (model, &parent_iter, &selected_iter);           // Get an iter corresponding to the currently selected iter's parent;

    selected_block = selected_block->parent;                                    // Move 'selected_block' one level up the tree (replace with parent);
    selected_iter = parent_iter;                                                // Move 'selected_iter' one level up the tree (replace with parent);
  }

  gtk_widget_destroy (GTK_WIDGET (wlist[1]));                                   // Destroy the import block list dialog;

  gcode_free ((gcode_t *)wlist[3]);                                             // Free the graphic context;
  free (wlist[3]);
  free (wlist);

  gui_collect_endmills_of (gui, NULL);                                          // Add any newly imported tools to the tool list;

  update_project_modified_flag (gui, 1);                                        // Finally, mark the project as 'changed'.
}

/**
 * Take the temporary object 'gcode', construct a dialog listing the top-level
 * blocks found in it, and hook up suitable handlers for the dialog's "cancel"
 * and "import" events, supplied with a context list of the relevant objects;
 */

static void
import_gcam_callback (gui_t *gui, gcode_t *gcode)
{
  gcode_block_t *index_block;
  gcode_t *imported;
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *sw;
  GtkListStore *store;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *treeview;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *cancel_button;
  GtkWidget *import_button;
  GtkWidget **wlist;
  int first_block;

  imported = (gcode_t *)gcode;

  gcode_prep (imported);

  /* Create window list of blocks to choose from */
  wlist = malloc (4 * sizeof (GtkWidget *));

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (gui->window));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);
  gtk_window_set_title (GTK_WINDOW (window), "Select Block to Import");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_table_set_row_spacings (GTK_TABLE (table), TABLE_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_container_add (GTK_CONTAINER (window), table);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_table_attach_defaults (GTK_TABLE (table), sw, 0, 2, 0, 1);
  gtk_widget_set_size_request (GTK_WIDGET (sw), 400, 240);

  /* create list store */
  store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

  /* create tree model */
  model = GTK_TREE_MODEL (store);

  treeview = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
  g_object_unref (model);
  gtk_container_add (GTK_CONTAINER (sw), treeview);

  first_block = 1;
  index_block = imported->listhead;

  while (index_block)
  {
    if (index_block->type != GCODE_TYPE_BEGIN && index_block->type != GCODE_TYPE_END && index_block->type != GCODE_TYPE_TOOL)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, GCODE_TYPE_STRING[index_block->type], 1, index_block->comment, 2, index_block, -1);

      if (first_block)
      {
        gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), &iter);
        first_block = 0;
      }
    }

    index_block = index_block->next;
  }

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Type", renderer, "text", 0, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 0);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Comment", renderer, "text", 1, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 1);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  wlist[0] = (GtkWidget *)gui;
  wlist[1] = window;
  wlist[2] = treeview;
  wlist[3] = (GtkWidget *)imported;

  cancel_button = gtk_button_new_with_label ("Cancel");
  gtk_table_attach_defaults (GTK_TABLE (table), cancel_button, 0, 1, 1, 2);
  g_signal_connect (cancel_button, "clicked", G_CALLBACK (cancel_import_gcam_callback), wlist);

  import_button = gtk_button_new_with_label ("Import");
  gtk_table_attach_defaults (GTK_TABLE (table), import_button, 1, 2, 1, 2);
  g_signal_connect (import_button, "clicked", G_CALLBACK (import_gcam_block_callback), wlist);

  gtk_widget_show_all (window);
}

/**
 * Select a GCAM project file to be imported, attempt importing its contents as
 * either binary or xml, and if successful, present a list of objects to import
 * then actually clone the selected object (with children) into the main gcode;
 * NOTE: This is a callback for the "Import GCAM" item of the "File" menu
 */

void
gui_menu_file_import_gcam_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gui_t *gui;
  char *filename;
  gcode_t *imported;

  gui = (gui_t *)data;

  dialog = gtk_file_chooser_dialog_new ("Import Blocks from GCAM File",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM project (*.gcam,*.gcamx)");
  gtk_file_filter_add_pattern (filter, "*.gcam");
  gtk_file_filter_add_pattern (filter, "*.gcamx");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM binary (*.gcam)");
  gtk_file_filter_add_pattern (filter, "*.gcam");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GCAM xml (*.gcamx)");
  gtk_file_filter_add_pattern (filter, "*.gcamx");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All files (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    gtk_widget_destroy (dialog);

    imported = malloc (sizeof (gcode_t));

    gcode_init (imported);

    imported->format = GCODE_FORMAT_BIN;

    if (gcode_load (imported, filename) != 0)
    {
      imported->format = GCODE_FORMAT_XML;

      if (gcode_load (imported, filename) != 0)
      {
        imported->format = GCODE_FORMAT_TBD;
      }
    }

    if (imported->format == GCODE_FORMAT_TBD)
    {
      generic_error (gui, "\nSomething went wrong - failed to load the file\n");

      free (imported);
    }
    else
    {
      import_gcam_callback (gui, imported);
    }

    g_free (filename);
  }
  else
  {
    gtk_widget_destroy (dialog);
  }
}

/**
 * Destroy/free any dynamic resources related to the Gerber import assistant;
 * NOTE: This is a callback for the Gerber import assistant's "cancel" event,
 * but it is also called after a normal "apply", when the assistant closes.
 */

static void
gerber_on_assistant_close_cancel (GtkWidget *assistant, gpointer data)
{
  GtkWidget **wlist;

  wlist = (GtkWidget **)data;

  gtk_widget_destroy (assistant);

  free (wlist);
}

/**
 * Init/update the current page of the Gerber import assistant on a page change;
 * NOTE: This is a callback for the Gerber import assistant's "prepare" event.
 */

static void
gerber_on_assistant_prepare (GtkWidget *assistant, GtkWidget *page, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  char *text_field;
  char tool_name[32];
  gui_endmill_t *endmill;
  gfloat_t tool_diameter;
  gfloat_t number_passes;
  gfloat_t pass_overlap;
  gfloat_t total_width;
  gint current_page;
  gint number_pages;
  gchar *title;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));
  strcpy (tool_name, &text_field[6]);
  g_free (text_field);

  endmill = gui_endmills_find (&gui->endmills, tool_name, TRUE);

  tool_diameter = gui_endmills_size (endmill, gui->gcode.units);

  current_page = gtk_assistant_get_current_page (GTK_ASSISTANT (assistant));
  number_pages = gtk_assistant_get_n_pages (GTK_ASSISTANT (assistant));

  title = g_strdup_printf ("Import RS274X (Gerber) - Step %d of %d", current_page + 1, number_pages);
  gtk_window_set_title (GTK_WINDOW (assistant), title);
  g_free (title);

  number_passes = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));
  pass_overlap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));

  total_width = (1 + (number_passes - 1) * (1 - pass_overlap)) * tool_diameter;

  /* Please note - the range below depends on the range of the other fields */

  SIGNAL_HANDLER_BLOCK_USING_DATA (G_OBJECT (wlist[7]), wlist);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (wlist[7]), tool_diameter, 10 * tool_diameter);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[7]), total_width);
  SIGNAL_HANDLER_UNBLOCK_USING_DATA (G_OBJECT (wlist[7]), wlist);
}

/**
 * Take the data (including a selected file name) collected by the Gerber import
 * assistant, perform the actual import into as many new sketches as required by
 * the isolation settings / tool width, then insert a new tool block and all the
 * new sketches into a new template block and insert that template block after 
 * the currently selected object (or a suitable parent) in the main tree view;
 * Notably, in this context, "import" implies creation of a number successively 
 * widening isolation contours around the outline of the imported Gerber traces;
 * NOTE: This is a callback for the Gerber import assistant's "apply" event.
 */

static void
gerber_on_assistant_apply (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  GtkTreeModel *model;
  GtkTreeIter parent_iter, selected_iter;
  gcode_block_t *template_block, *tool_block, *sketch_block, *selected_block;
  gcode_tool_t *tool;
  gui_endmill_t *endmill;
  gfloat_t tool_diameter, pass_count, pass_overlap, pass_depth, pass_offset;
  uint8_t tool_number;
  gcode_vec2d_t aabb_min, aabb_max;
  char *text_field, tool_name[32], filename[256];
  int import_failed;

  wlist = (GtkWidget **)data;                                                   // Retrieve a reference to the GUI context;

  gui = (gui_t *)wlist[0];                                                      // Using that, retrieve a reference to 'gui';

  gtk_widget_hide (assistant);                                                  // Ask GTK to hide the assistant;
  gtk_main_iteration ();                                                        // Let GTK actually execute that;

  strcpy (filename, gtk_entry_get_text (GTK_ENTRY (wlist[1])));

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));
  strcpy (tool_name, &text_field[6]);
  g_free (text_field);

  endmill = gui_endmills_find (&gui->endmills, tool_name, TRUE);

  tool_diameter = gui_endmills_size (endmill, gui->gcode.units);
  tool_number = endmill->number;

  gcode_template_init (&template_block, &gui->gcode, NULL);                     // Create a new template block to import things into;

  sprintf (template_block->comment, "Gerber layer from '%s'", basename ((char *)filename));

  gcode_tool_init (&tool_block, &gui->gcode, template_block);                   // Create a new tool to perform the etching with,

  gcode_insert_as_listhead (template_block, tool_block);                        // and add the tool to the list of the template;

  tool = (gcode_tool_t *)tool_block->pdata;

  tool->feed = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[3]));
  tool->diameter = tool_diameter;
  tool->number = tool_number;
  strcpy (tool->label, tool_name);

  pass_depth = -gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[4]));
  pass_count = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));
  pass_overlap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));

  pass_offset = tool_diameter / 2;

  import_failed = 0;

  for (int i = 0; i < pass_count && !import_failed; i++)                        // For each isolation pass (unless any one of them fails),
  {
    gcode_sketch_init (&sketch_block, &gui->gcode, template_block);             // create a new sketch to import that pass into,

    gcode_append_as_listtail (template_block, sketch_block);                    // and add the sketch to the list of the template;

    import_failed = gcode_gerber_import (sketch_block, filename, pass_depth, pass_offset);

    pass_offset += (1 - pass_overlap) * tool_diameter;
  }

  if (import_failed)                                                            // In case of failure, undo everything (free the template recursively);
  {
    generic_error (gui, "\nSomething went wrong - failed to import the file\n");
    template_block->free (&template_block);
    return;
  }

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (gui->gcode_block_treeview));  // Retrieve a reference to the model of the main GUI tree view;

  get_selected_block (gui, &selected_block, &selected_iter);                    // Get the currently selected block and iter in the main GUI tree view;

  while (selected_block)                                                        // Must back up the tree if the new block is not valid after the selected one;
  {
    if (selected_block->parent)                                                 // Does the selected block have a parent or is it top level?
    {
      if (GCODE_IS_VALID_PARENT_CHILD[selected_block->parent->type][template_block->type])      // If it has one, check validity of the template block under it;
      {
        insert_primitive (gui, template_block, selected_block, &selected_iter, GUI_INSERT_AFTER);       // If valid, insert it and leave;
        break;
      }
    }
    else
    {
      if (GCODE_IS_VALID_IF_NO_PARENT[template_block->type])                    // If no parent, check validity of the template block as a top level block;
      {
        insert_primitive (gui, template_block, selected_block, &selected_iter, GUI_INSERT_AFTER);       // If valid, insert it and leave;
        break;
      }
    }

    gtk_tree_model_iter_parent (model, &parent_iter, &selected_iter);           // Get an iter corresponding to the currently selected iter's parent;

    selected_block = selected_block->parent;                                    // Move 'selected_block' one level up the tree (replace with parent);
    selected_iter = parent_iter;                                                // Move 'selected_iter' one level up the tree (replace with parent);
  }

  /* Get the bounding box for the template */
  template_block->aabb (template_block, aabb_min, aabb_max);

  /* Extend material size and/or move origin if anything spills over */
  if ((aabb_min[0] < aabb_max[0]) && (aabb_min[1] < aabb_max[1]))
  {
    if (gui->gcode.material_origin[0] + aabb_min[0] < 0)
      gui->gcode.material_origin[0] = -aabb_min[0];

    if (gui->gcode.material_origin[1] + aabb_min[1] < 0)
      gui->gcode.material_origin[1] = -aabb_min[1];

    if (gui->gcode.material_size[0] < (aabb_max[0] - aabb_min[0]))
      gui->gcode.material_size[0] = aabb_max[0] - aabb_min[0];

    if (gui->gcode.material_size[1] < (aabb_max[1] - aabb_min[1]))
      gui->gcode.material_size[1] = aabb_max[1] - aabb_min[1];
  }

  gui_opengl_build_gridxy_display_list (&gui->opengl);
  gui_opengl_build_gridxz_display_list (&gui->opengl);

  gcode_prep (&gui->gcode);

  gui->opengl.rebuild_view_display_list = 1;
  gui_opengl_context_redraw (&gui->opengl, selected_block);
}

/**
 * Validate the first page of the Gerber import assistant (basically, tell the
 * assistant whether it can enable the "next" button) based on existence of text
 * in the file name selection text-field. Any text entered validates the page.
 * NOTE: This is a callback for the Gerber import assistant's "changed" event
 */

static void
gerber_on_entry_changed (GtkWidget *widget, gpointer data)
{
  GtkAssistant *assistant;
  GtkWidget *current_page;
  gint page_number;
  const gchar *text;

  assistant = GTK_ASSISTANT (data);

  page_number = gtk_assistant_get_current_page (assistant);
  current_page = gtk_assistant_get_nth_page (assistant, page_number);
  text = gtk_entry_get_text (GTK_ENTRY (widget));

  if (text && *text)
  {
    gtk_assistant_set_page_complete (assistant, current_page, TRUE);
  }
  else
  {
    gtk_assistant_set_page_complete (assistant, current_page, FALSE);
  }
}

/**
 * Select a file name for the Gerber file to be imported under the Gerber import
 * assistant and deposit it into the text field on the assistant's first page;
 * NOTE: This is a callback for the assistant's browse button's "clicked" event
 */

static void
gerber_browse_file_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget *dialog;
  GtkWidget **wlist;
  GtkFileFilter *filter;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  dialog = gtk_file_chooser_dialog_new ("Select Gerber File",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "RS274X (*.gbr,*.gbx,*.gtl,*.gbl,*.art,*.pho)");
  gtk_file_filter_add_pattern (filter, "*.gbr");
  gtk_file_filter_add_pattern (filter, "*.gbx");
  gtk_file_filter_add_pattern (filter, "*.gtl");
  gtk_file_filter_add_pattern (filter, "*.gbl");
  gtk_file_filter_add_pattern (filter, "*.art");
  gtk_file_filter_add_pattern (filter, "*.pho");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    gtk_entry_set_text (GTK_ENTRY (wlist[1]), filename);

    g_free (filename);
  }

  gtk_widget_destroy (dialog);
}

static void
gerber_on_spin_changed (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget **wlist;
  char *text_field;
  char tool_name[32];
  gui_endmill_t *endmill;
  gfloat_t tool_diameter;
  gfloat_t number_passes;
  gfloat_t pass_overlap;
  gfloat_t total_width;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));
  strcpy (tool_name, &text_field[6]);
  g_free (text_field);

  endmill = gui_endmills_find (&gui->endmills, tool_name, TRUE);

  tool_diameter = gui_endmills_size (endmill, gui->gcode.units);

  if (widget == GTK_WIDGET (wlist[5]))
  {
    number_passes = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));
    pass_overlap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));

    total_width = (1 + (number_passes - 1) * (1 - pass_overlap)) * tool_diameter;

    SIGNAL_HANDLER_BLOCK_USING_DATA (G_OBJECT (wlist[7]), wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[7]), total_width);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (G_OBJECT (wlist[7]), wlist);
  }
  else if (widget == GTK_WIDGET (wlist[6]))
  {
    number_passes = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[5]));
    pass_overlap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));

    total_width = (1 + (number_passes - 1) * (1 - pass_overlap)) * tool_diameter;

    SIGNAL_HANDLER_BLOCK_USING_DATA (G_OBJECT (wlist[7]), wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[7]), total_width);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (G_OBJECT (wlist[7]), wlist);
  }
  else if (widget == GTK_WIDGET (wlist[7]))
  {
    pass_overlap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[6]));
    total_width = gtk_spin_button_get_value (GTK_SPIN_BUTTON (wlist[7]));

    number_passes = ceil ((total_width / tool_diameter - 1) / (1 - pass_overlap) + 1);
    pass_overlap = 1 - (total_width / tool_diameter - 1) / (number_passes - 1);

    while ((number_passes > 10) && (pass_overlap > 0))
    {
      pass_overlap = (pass_overlap >= 0.1) ? pass_overlap - 0.1 : 0.0;

      number_passes = ceil ((total_width / tool_diameter - 1) / (1 - pass_overlap) + 1);
      pass_overlap = 1 - (total_width / tool_diameter - 1) / (number_passes - 1);
    }

    SIGNAL_HANDLER_BLOCK_USING_DATA (G_OBJECT (wlist[5]), wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[5]), number_passes);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (G_OBJECT (wlist[5]), wlist);

    SIGNAL_HANDLER_BLOCK_USING_DATA (G_OBJECT (wlist[6]), wlist);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (wlist[6]), pass_overlap);
    SIGNAL_HANDLER_UNBLOCK_USING_DATA (G_OBJECT (wlist[6]), wlist);
  }
}

/**
 * Create the first page of the Gerber import assistant and hook up callbacks
 * for the browse button "click" and the text box "entry changed" events;
 */

static void
gerber_create_page1 (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *align1;
  GtkWidget *align2;
  GtkWidget *label;
  GtkWidget *file_entry;
  GtkWidget *browse_button;
  GtkWidget **wlist;
  GdkPixbuf *pixbuf;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  vbox = gtk_vbox_new (FALSE, TABLE_SPACING);                                   // New vertical 3-cell box 'vbox'
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER_WIDTH);

  align1 = gtk_alignment_new (0, 1, 0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align1, TRUE, TRUE, 0);                   // 'vbox' cell 1 <- spacer 'align1'

  hbox = gtk_hbox_new (FALSE, TABLE_SPACING);                                   // New horizontal 2-cell box 'hbox'
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);                   // 'vbox' cell 2 <- horizontal box 'hbox'

  align2 = gtk_alignment_new (0, 0, 1, 0);
  gtk_box_pack_start (GTK_BOX (vbox), align2, TRUE, TRUE, 0);                   // 'vbox' cell 3 <- spacer 'align2'

  label = gtk_label_new ("File to import:");
  gtk_container_add (GTK_CONTAINER (align1), label);                            // 'align1' <- label 'label'

  file_entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (file_entry), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), file_entry, TRUE, TRUE, 0);               // 'hbox' cell 1 <- entry field 'file_entry'

  g_signal_connect (file_entry, "changed", G_CALLBACK (gerber_on_entry_changed), assistant);

  browse_button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
  gtk_box_pack_start (GTK_BOX (hbox), browse_button, FALSE, FALSE, 0);          // 'hbox' cell 2 <- button 'browse_button'

  g_signal_connect (browse_button, "clicked", G_CALLBACK (gerber_browse_file_callback), wlist);

  wlist[1] = file_entry;

  gtk_widget_show_all (vbox);

  gtk_assistant_append_page (GTK_ASSISTANT (assistant), vbox);
  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), vbox, "Select File to Import");
  gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), vbox, GTK_ASSISTANT_PAGE_INTRO);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), vbox, FALSE);

  pixbuf = gtk_widget_render_icon (assistant, GTK_STOCK_OPEN, GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_assistant_set_page_header_image (GTK_ASSISTANT (assistant), vbox, pixbuf);
  g_object_unref (pixbuf);
}

/**
 * Create the second page of the Gerber import assistant (no callbacks needed )
 */

static void
gerber_create_page2 (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget *vbox1;
  GtkWidget *vbox2;
  GtkWidget *hbox1;
  GtkWidget *hbox2;
  GtkWidget *hbox3;
  GtkWidget *label;
  GtkWidget *tool_combo;
  GtkWidget *feed_spin;
  GtkWidget *depth_spin;
  GtkWidget **wlist;
  GdkPixbuf *pixbuf;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  vbox1 = gtk_vbox_new (FALSE, BORDER_WIDTH);                                   // New vertical 2-cell box 'vbox1'
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), BORDER_WIDTH);

  label = gtk_label_new ("The end mill diameter affects the RS274X to G-Code conversion process. "
                         "Changing this diameter after the conversion will not update the sketches. "
                         "The best tool to use is a \"V\"-tip engraving toolbit with a fine point.");

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);                   // 'vbox1' cell 1 <- label 'label'

  vbox2 = gtk_vbox_new (FALSE, TABLE_SPACING);                                  // New vertical 3-cell box 'vbox2' (to space other controls away from 'label')
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 0);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);                 // 'vbox1' cell 2 <- vertical box 'vbox2'

  hbox1 = gtk_hbox_new (TRUE, 0);                                               // New horizontal 2-cell box 'hbox1' (to space label and control 50% : 50%)
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);                 // 'vbox2' cell 1 <- horizontal box 'hbox1'

  hbox2 = gtk_hbox_new (TRUE, 0);                                               // New horizontal 2-cell box 'hbox2'
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);                 // 'vbox2' cell 2 <- horizontal box 'hbox2'

  hbox3 = gtk_hbox_new (TRUE, 0);                                               // New horizontal 2-cell box 'hbox3'
  gtk_container_set_border_width (GTK_CONTAINER (hbox3), 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);                 // 'vbox2' cell 3 <- horizontal box 'hbox3'

  {
    char string[32];
    int i;

    label = gtk_label_new ("End Mill");
    gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);                 // 'hbox1' cell 1 <- label 'label'

    tool_combo = gtk_combo_box_new_text ();

    for (i = 0; i < gui->endmills.number; i++)
    {
      if (gui->endmills.endmill[i].origin == GUI_ENDMILL_INTERNAL)
      {
        sprintf (string, "T%.2d - %s", gui->endmills.endmill[i].number, gui->endmills.endmill[i].description);
        gtk_combo_box_append_text (GTK_COMBO_BOX (tool_combo), string);
      }
    }

    gtk_combo_box_set_active (GTK_COMBO_BOX (tool_combo), 0);
    gtk_box_pack_start (GTK_BOX (hbox1), tool_combo, TRUE, TRUE, 0);            // 'hbox1' cell 2 <- combo 'tool_combo'
  }

  label = gtk_label_new ("Feed Rate");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);                   // 'hbox2' cell 1 <- label 'label'

  feed_spin = gtk_spin_button_new_with_range (SCALED_INCHES (0.01), SCALED_INCHES (30.0), SCALED_INCHES (1.0));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (feed_spin), 2);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (feed_spin), SCALED_INCHES (10.0));
  gtk_box_pack_start (GTK_BOX (hbox2), feed_spin, TRUE, TRUE, 0);               // 'hbox2' cell 2 <- spin 'feed_spin'

  gtk_widget_set_tooltip_text (feed_spin, GCAM_TTIP_IMPORT_GERBER_FEED);

  g_signal_connect_swapped (feed_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  label = gtk_label_new ("Cutting Depth");
  gtk_box_pack_start (GTK_BOX (hbox3), label, TRUE, TRUE, 0);                   // 'hbox3' cell 1 <- label 'label'

  depth_spin = gtk_spin_button_new_with_range (-gui->gcode.material_size[2], SCALED_INCHES (0.0), SCALED_INCHES (0.0001));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (depth_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (depth_spin), SCALED_INCHES (-0.0024));
  gtk_box_pack_start (GTK_BOX (hbox3), depth_spin, TRUE, TRUE, 0);              // 'hbox3' cell 2 <- spin 'depth_spin'

  gtk_widget_set_tooltip_text (depth_spin, GCAM_TTIP_IMPORT_GERBER_DEPTH);

  g_signal_connect_swapped (depth_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  wlist[2] = tool_combo;
  wlist[3] = feed_spin;
  wlist[4] = depth_spin;

  gtk_widget_show_all (vbox1);

  gtk_assistant_append_page (GTK_ASSISTANT (assistant), vbox1);
  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), vbox1, "Etching Parameters");
  gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), vbox1, GTK_ASSISTANT_PAGE_CONTENT);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), vbox1, TRUE);

  pixbuf = gtk_widget_render_icon (assistant, GTK_STOCK_OPEN, GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_assistant_set_page_header_image (GTK_ASSISTANT (assistant), vbox1, pixbuf);
  g_object_unref (pixbuf);
}

/**
 * Create the third page of the Gerber import assistant (no callbacks needed )
 */

static void
gerber_create_page3 (GtkWidget *assistant, gpointer data)
{
  gui_t *gui;
  GtkWidget *vbox1;
  GtkWidget *vbox2;
  GtkWidget *hbox1;
  GtkWidget *hbox2;
  GtkWidget *hbox3;
  GtkWidget *label;
  GtkWidget *passes_spin;
  GtkWidget *overlap_spin;
  GtkWidget *width_spin;
  GtkWidget **wlist;
  GdkPixbuf *pixbuf;
  char *text_field;
  char tool_name[32];
  gui_endmill_t *endmill;
  gfloat_t tool_diameter;

  wlist = (GtkWidget **)data;

  gui = (gui_t *)wlist[0];

  text_field = gtk_combo_box_get_active_text (GTK_COMBO_BOX (wlist[2]));
  strcpy (tool_name, &text_field[6]);
  g_free (text_field);

  endmill = gui_endmills_find (&gui->endmills, tool_name, TRUE);

  tool_diameter = gui_endmills_size (endmill, gui->gcode.units);

  vbox1 = gtk_vbox_new (FALSE, BORDER_WIDTH);                                   // New vertical 2-cell box 'vbox1'
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), BORDER_WIDTH);

  label = gtk_label_new ("The isolation settings control the separation between traces and surrounding copper. "
                         "The tool makes partially overlapping passes until the target gap width is reached. "
                         "A sketch for each isolation pass will be generated.");

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);                   // 'vbox1' cell 1 <- label 'label'

  vbox2 = gtk_vbox_new (FALSE, TABLE_SPACING);                                  // New vertical 3-cell box 'vbox2' (to space other controls away from 'label')
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 0);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);                 // 'vbox1' cell 2 <- vertical box 'vbox2'

  hbox1 = gtk_hbox_new (TRUE, 0);                                               // New horizontal 2-cell box 'hbox1' (to space label and control 50% : 50%)
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);                 // 'vbox2' cell 1 <- horizontal box 'hbox1'

  hbox2 = gtk_hbox_new (TRUE, 0);                                               // New horizontal 2-cell box 'hbox2'
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);                 // 'vbox2' cell 2 <- horizontal box 'hbox2'

  hbox3 = gtk_hbox_new (TRUE, 0);                                               // New horizontal 2-cell box 'hbox3'
  gtk_container_set_border_width (GTK_CONTAINER (hbox3), 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);                 // 'vbox2' cell 3 <- horizontal box 'hbox3'

  label = gtk_label_new ("Number of Passes");
  gtk_box_pack_start (GTK_BOX (hbox1), label, TRUE, TRUE, 0);                   // 'hbox1' cell 1 <- label 'label'

  passes_spin = gtk_spin_button_new_with_range (1.0, 10.0, 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (passes_spin), 0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (passes_spin), 1.0);
  gtk_box_pack_start (GTK_BOX (hbox1), passes_spin, TRUE, TRUE, 0);             // 'hbox1' cell 2 <- spin 'passes_spin'

  gtk_widget_set_tooltip_text (passes_spin, GCAM_TTIP_IMPORT_GERBER_PASSES);

  g_signal_connect (passes_spin, "value-changed", G_CALLBACK (gerber_on_spin_changed), wlist);
  g_signal_connect_swapped (passes_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  label = gtk_label_new ("Pass Overlap");
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);                   // 'hbox2' cell 1 <- label 'label'

  overlap_spin = gtk_spin_button_new_with_range (0.0, 1.0, 0.1);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (overlap_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (overlap_spin), 0.5);
  gtk_box_pack_start (GTK_BOX (hbox2), overlap_spin, TRUE, TRUE, 0);            // 'hbox2' cell 2 <- spin 'overlap_spin'

  gtk_widget_set_tooltip_text (overlap_spin, GCAM_TTIP_IMPORT_GERBER_OVERLAP);

  g_signal_connect (overlap_spin, "value-changed", G_CALLBACK (gerber_on_spin_changed), wlist);
  g_signal_connect_swapped (overlap_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  label = gtk_label_new ("Total Gap Width");
  gtk_box_pack_start (GTK_BOX (hbox3), label, TRUE, TRUE, 0);                   // 'hbox3' cell 1 <- label 'label'

  /* Please note - the defaults below should depend on the defaults above */

  width_spin = gtk_spin_button_new_with_range (tool_diameter, tool_diameter * 10, SCALED_INCHES (0.001));
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (width_spin), MANTISSA);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (width_spin), tool_diameter);
  gtk_box_pack_start (GTK_BOX (hbox3), width_spin, TRUE, TRUE, 0);              // 'hbox3' cell 2 <- spin 'width_spin'

  gtk_widget_set_tooltip_text (width_spin, GCAM_TTIP_IMPORT_GERBER_WIDTH);

  g_signal_connect (width_spin, "value-changed", G_CALLBACK (gerber_on_spin_changed), wlist);
  g_signal_connect_swapped (width_spin, "activate", G_CALLBACK (gtk_window_activate_default), assistant);

  wlist[5] = passes_spin;
  wlist[6] = overlap_spin;
  wlist[7] = width_spin;

  gtk_widget_show_all (vbox1);

  gtk_assistant_append_page (GTK_ASSISTANT (assistant), vbox1);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), vbox1, TRUE);
  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), vbox1, "Isolation Details");
  gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), vbox1, GTK_ASSISTANT_PAGE_CONFIRM);

  pixbuf = gtk_widget_render_icon (assistant, GTK_STOCK_OPEN, GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_assistant_set_page_header_image (GTK_ASSISTANT (assistant), vbox1, pixbuf);
  g_object_unref (pixbuf);
}

/**
 * Construct a Gtk "assistant" (wizard) that allows configuration of the various
 * Gerber import parameters (including choosing the file to import), and hook up 
 * handlers for the assistant's "cancel", "close", "prepare" and "apply" events;
 * NOTE: This is a callback for the "Import Gerber" item of the "File" menu
 */

void
gui_menu_file_import_gerber_menuitem_callback (GtkWidget *widget, gpointer data)
{
  gui_t *gui;
  GtkWidget *assistant;
  GtkWidget **wlist;

  gui = (gui_t *)data;

  assistant = gtk_assistant_new ();

  gtk_window_set_default_size (GTK_WINDOW (assistant), -1, -1);
  gtk_window_set_screen (GTK_WINDOW (assistant), gtk_widget_get_screen (gui->window));
  gtk_window_set_transient_for (GTK_WINDOW (assistant), GTK_WINDOW (gui->window));

  /* Setup Global Widgets */
  wlist = malloc (8 * sizeof (GtkWidget *));

  wlist[0] = (void *)gui;

  gerber_create_page1 (assistant, wlist);
  gerber_create_page2 (assistant, wlist);
  gerber_create_page3 (assistant, wlist);

  g_signal_connect (assistant, "cancel", G_CALLBACK (gerber_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "close", G_CALLBACK (gerber_on_assistant_close_cancel), wlist);
  g_signal_connect (assistant, "prepare", G_CALLBACK (gerber_on_assistant_prepare), wlist);
  g_signal_connect (assistant, "apply", G_CALLBACK (gerber_on_assistant_apply), wlist);

  gtk_widget_show (assistant);
}

/**
 * Select an Excellon drill file to be imported, attempt importing its contents
 * into a new template block and insert that template block after the currently
 * selected object (or a suitable parent) in the main tree view;
 * NOTE: This is a callback for the "Import Excellon" item of the "File" menu
 */

void
gui_menu_file_import_excellon_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gui_t *gui;

  gui = (gui_t *)data;                                                          // Retrieve a reference to 'gui'

  dialog = gtk_file_chooser_dialog_new ("Load Excellon Drill File",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);                                  // Construct a file selector dialog;

  filter = gtk_file_filter_new ();                                              // Add a filter for Excellon files,
  gtk_file_filter_set_name (filter, "Excellon (*.nc,*.drl)");
  gtk_file_filter_add_pattern (filter, "*.nc");
  gtk_file_filter_add_pattern (filter, "*.drl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();                                              // and another one for all files;
  gtk_file_filter_set_name (filter, "All (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)                                                     // If there's a known previously used folder, open directly there;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)              // If a selection was made in the dialog,
  {
    GtkTreeModel *model;
    GtkTreeIter parent_iter, selected_iter;
    gcode_block_t *template_block, *selected_block;
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));       // fetch the resulting filename;

    gcode_template_init (&template_block, &gui->gcode, NULL);                   // Create a new template block to import things into;

    if (gcode_excellon_import (template_block, filename) != 0)                  // Try to import the file and abort if that fails;
    {
      generic_error (gui, "\nSomething went wrong - failed to import the file\n");
      template_block->free (&template_block);
      g_free (filename);
      gtk_widget_destroy (dialog);
      return;
    }

    sprintf (template_block->comment, "Excellon layer from '%s'", basename ((char *)filename)); // Create a block comment that mentions the filename;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (gui->gcode_block_treeview));        // Retrieve a reference to the model of the main GUI tree view;

    get_selected_block (gui, &selected_block, &selected_iter);                  // Get the currently selected block and iter in the main GUI tree view;

    /* This would not be so elaborate with a more context-aware "File" menu */

    while (selected_block)                                                      // Must back up the tree if the new block is not valid after the selected one;
    {
      if (selected_block->parent)                                               // Does the selected block have a parent or is it top level?
      {
        if (GCODE_IS_VALID_PARENT_CHILD[selected_block->parent->type][template_block->type])    // If it has one, check validity of the template block under it;
        {
          insert_primitive (gui, template_block, selected_block, &selected_iter, GUI_INSERT_AFTER);     // If valid, insert it and leave;
          break;
        }
      }
      else
      {
        if (GCODE_IS_VALID_IF_NO_PARENT[template_block->type])                  // If no parent, check validity of the template block as a top level block;
        {
          insert_primitive (gui, template_block, selected_block, &selected_iter, GUI_INSERT_AFTER);     // If valid, insert it and leave;
          break;
        }
      }

      gtk_tree_model_iter_parent (model, &parent_iter, &selected_iter);         // Get an iter corresponding to the currently selected iter's parent;

      selected_block = selected_block->parent;                                  // Move 'selected_block' one level up the tree (replace with parent);
      selected_iter = parent_iter;                                              // Move 'selected_iter' one level up the tree (replace with parent);
    }

    g_free (filename);

    gui_opengl_build_gridxy_display_list (&gui->opengl);
    gui_opengl_build_gridxz_display_list (&gui->opengl);

    gcode_prep (&gui->gcode);

    gui_collect_endmills_of (gui, NULL);                                        // Add any newly imported tools to the tool list;

    gui->opengl.rebuild_view_display_list = 1;
    gui_opengl_context_redraw (&gui->opengl, selected_block);
  }

  gtk_widget_destroy (dialog);
}

/**
 * Select a SVG vector path file to be imported, attempt importing its contents
 * into a new sketch block and then insert that sketch block after the currently
 * selected object (or a suitable parent) in the main tree view;
 * NOTE: This is a callback for the "Import SVG Paths" item of the "File" menu
 */

void
gui_menu_file_import_svg_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gui_t *gui;

  gui = (gui_t *)data;                                                          // Retrieve a reference to 'gui'

  dialog = gtk_file_chooser_dialog_new ("Import SVG",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);                                  // Construct a file selector dialog;

  filter = gtk_file_filter_new ();                                              // Add a filter for SVG files,
  gtk_file_filter_set_name (filter, "SVG (*.svg)");
  gtk_file_filter_add_pattern (filter, "*.svg");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();                                              // and another one for all files;
  gtk_file_filter_set_name (filter, "All (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)                                                     // If there's a known previously used folder, open directly there;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)              // If a selection was made in the dialog,
  {
    GtkTreeModel *model;
    GtkTreeIter parent_iter, selected_iter;
    gcode_block_t *template_block, *selected_block;
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));       // fetch the resulting filename;

    gcode_template_init (&template_block, &gui->gcode, NULL);                   // Create a new template block to import things into;

    if (gcode_svg_import (template_block, filename) != 0)                       // Try to import the file and abort if that fails;
    {
      generic_error (gui, "\nSomething went wrong - failed to import the file\n");
      template_block->free (&template_block);
      g_free (filename);
      gtk_widget_destroy (dialog);
      return;
    }

    sprintf (template_block->comment, "SVG layer from '%s'", basename ((char *)filename));      // Create a block comment that mentions the filename;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (gui->gcode_block_treeview));        // Retrieve a reference to the model of the main GUI tree view;

    get_selected_block (gui, &selected_block, &selected_iter);

    /* This would not be so elaborate with a more context-aware "File" menu */

    while (selected_block)                                                      // Must back up the tree if the new block is not valid after the selected one;
    {
      if (selected_block->parent)                                               // Does the selected block have a parent or is it top level?
      {
        if (GCODE_IS_VALID_PARENT_CHILD[selected_block->parent->type][template_block->type])    // If it has one, check validity of the template block under it;
        {
          insert_primitive (gui, template_block, selected_block, &selected_iter, GUI_INSERT_AFTER);     // If valid, insert it and leave;
          break;
        }
      }
      else
      {
        if (GCODE_IS_VALID_IF_NO_PARENT[template_block->type])                  // If no parent, check validity of the template block as a top level block;
        {
          insert_primitive (gui, template_block, selected_block, &selected_iter, GUI_INSERT_AFTER);     // If valid, insert it and leave;
          break;
        }
      }

      gtk_tree_model_iter_parent (model, &parent_iter, &selected_iter);         // Get an iter corresponding to the currently selected iter's parent;

      selected_block = selected_block->parent;                                  // Move 'selected_block' one level up the tree (replace with parent);
      selected_iter = parent_iter;                                              // Move 'selected_iter' one level up the tree (replace with parent);
    }

    g_free (filename);

    gui_opengl_build_gridxy_display_list (&gui->opengl);
    gui_opengl_build_gridxz_display_list (&gui->opengl);

    gcode_prep (&gui->gcode);

    gui->opengl.rebuild_view_display_list = 1;
    gui_opengl_context_redraw (&gui->opengl, selected_block);
  }

  gtk_widget_destroy (dialog);
}

/**
 * Select a STL object shape file to be imported, attempt importing its contents
 * into a new stl block then insert that stl block after the currently selected 
 * object (or a suitable parent) in the main tree view;
 * NOTE: This is a callback for the "Import STL" item of the "File" menu
 */

void
gui_menu_file_import_stl_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gui_t *gui;

  gui = (gui_t *)data;                                                          // Retrieve a reference to 'gui'

  dialog = gtk_file_chooser_dialog_new ("Import STL",
                                        GTK_WINDOW (gui->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);                                  // Construct a file selector dialog;

  filter = gtk_file_filter_new ();                                              // Add a filter for STL files,
  gtk_file_filter_set_name (filter, "STL (*.stl)");
  gtk_file_filter_add_pattern (filter, "*.stl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();                                              // and another one for all files;
  gtk_file_filter_set_name (filter, "All (*.*)");
  gtk_file_filter_add_pattern (filter, "*.*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (*gui->current_folder)                                                     // If there's a known previously used folder, open directly there;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), gui->current_folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)              // If a selection was made in the dialog,
  {
    GtkTreeModel *model;
    GtkTreeIter parent_iter, selected_iter;
    gcode_block_t *stl_block, *selected_block;
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));       // fetch the resulting filename;

    gcode_stl_init (&stl_block, &gui->gcode, NULL);                             // Create a new stl block to import things into;

    if (gcode_stl_import (stl_block, filename) != 0)                            // Try to import the file and abort if that fails;
    {
      generic_error (gui, "\nSomething went wrong - failed to import the file\n");
      stl_block->free (&stl_block);
      g_free (filename);
      gtk_widget_destroy (dialog);
      return;
    }

    sprintf (stl_block->comment, "STL layer from '%s'", basename ((char *)filename));   // Create a block comment that mentions the filename;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (gui->gcode_block_treeview));        // Retrieve a reference to the model of the main GUI tree view;

    get_selected_block (gui, &selected_block, &selected_iter);

    /* This would not be so elaborate with a more context-aware "File" menu */

    while (selected_block)                                                      // Must back up the tree if the new block is not valid after the selected one;
    {
      if (selected_block->parent)                                               // Does the selected block have a parent or is it top level?
      {
        if (GCODE_IS_VALID_PARENT_CHILD[selected_block->parent->type][stl_block->type]) // If it has one, check validity of the stl block under it;
        {
          insert_primitive (gui, stl_block, selected_block, &selected_iter, GUI_INSERT_AFTER);  // If valid, insert it and leave;
          break;
        }
      }
      else
      {
        if (GCODE_IS_VALID_IF_NO_PARENT[stl_block->type])                       // If no parent, check validity of the stl block as a top level block;
        {
          insert_primitive (gui, stl_block, selected_block, &selected_iter, GUI_INSERT_AFTER);  // If valid, insert it and leave;
          break;
        }
      }

      gtk_tree_model_iter_parent (model, &parent_iter, &selected_iter);         // Get an iter corresponding to the currently selected iter's parent;

      selected_block = selected_block->parent;                                  // Move 'selected_block' one level up the tree (replace with parent);
      selected_iter = parent_iter;                                              // Move 'selected_iter' one level up the tree (replace with parent);
    }

    g_free (filename);

    gui_opengl_build_gridxy_display_list (&gui->opengl);
    gui_opengl_build_gridxz_display_list (&gui->opengl);

    gcode_prep (&gui->gcode);

    gui->opengl.rebuild_view_display_list = 1;
    gui_opengl_context_redraw (&gui->opengl, selected_block);
  }

  gtk_widget_destroy (dialog);
}

/**
 * Depending on the chosen response, do or do not save the current project under
 * either the file name stored within 'gui' or a newly selected one then discard
 * (free) all blocks of the current project then call in the demolishing squad;
 * NOTE: This is a callback for the "response" event of the quit dialog
 */

static void
quit_callback (GtkDialog *dialog, gint response, gpointer data)
{
  gui_t *gui;
  gint result;

  gui = (gui_t *)data;

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (response == GTK_RESPONSE_CANCEL)
    return;

  if (response == GTK_RESPONSE_DELETE_EVENT)
    return;

  if (response == GTK_RESPONSE_ACCEPT)
  {
    if (*gui->filename)
    {
      gcode_save (&gui->gcode, gui->filename);
    }
    else
    {
      result = gui_menu_file_save_project_as_menuitem_callback (NULL, gui);

      if (result != GTK_RESPONSE_ACCEPT)
        return;
    }
  }

  gcode_free (&gui->gcode);
  gui_destroy ();
}

/**
 * If the project is modified, construct a dialog asking whether or not to save
 * it then hook up a suitable callback for the dialog's "response" event; if the
 * project needs no saving, discard (free) all blocks of the current project and
 * call in the demolishing squad;
 * NOTE: This is a callback for the "Quit" item of the "File" menu
 */

void
gui_menu_file_quit_menuitem_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog;
  gui_t *gui;

  gui = (gui_t *)data;

  if (gui->project_state == PROJECT_CLOSED)
  {
    gcode_free (&gui->gcode);
    gui_destroy ();
    return;
  }

  if (!gui->modified)
  {
    gcode_free (&gui->gcode);
    gui_destroy ();
    return;
  }

  dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gui->window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_WARNING,
                                               GTK_BUTTONS_NONE,
                                               GCAM_CLOSE_LINE1,
                                               gui->gcode.name);

  gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
                                              GCAM_CLOSE_LINE2);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GCAM_STOCK_CLOSE, GTK_RESPONSE_REJECT);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  if (*gui->filename)
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT);
  else
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gui->window));
  g_signal_connect (dialog, "response", G_CALLBACK (quit_callback), gui);

  gtk_widget_show (dialog);
}
