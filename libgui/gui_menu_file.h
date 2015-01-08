/**
 *  gui_menu_file.h
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

#ifndef _GUI_MENU_FILE_H
#define _GUI_MENU_FILE_H

#include <gtk/gtk.h>

void gui_menu_file_new_project_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_load_project_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_save_project_menuitem_callback (GtkWidget *widget, gpointer data);
gint gui_menu_file_save_project_as_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_close_project_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_export_gcode_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_import_gcam_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_import_gerber_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_import_excellon_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_import_svg_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_import_stl_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_file_quit_menuitem_callback (GtkWidget *widget, gpointer data);

#endif
