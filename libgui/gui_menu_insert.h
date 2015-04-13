/**
 *  gui_menu_insert.h
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

#ifndef _GUI_MENU_INSERT_H
#define _GUI_MENU_INSERT_H

#include <gtk/gtk.h>

void gui_menu_insert_tool_change_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_template_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_sketch_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_arc_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_line_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_bolt_holes_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_drill_holes_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_point_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_insert_image_menuitem_callback (GtkWidget *widget, gpointer data);

#endif
