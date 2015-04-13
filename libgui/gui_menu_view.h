/**
 *  gui_menu_view.h
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

#ifndef _GUI_MENU_VIEW_H
#define _GUI_MENU_VIEW_H

#include <gtk/gtk.h>

void gui_menu_view_perspective_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_view_orthographic_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_view_top_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_view_left_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_view_right_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_view_front_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_view_back_menuitem_callback (GtkWidget *widget, gpointer data);
void gui_menu_view_render_final_part_menuitem_callback (GtkWidget *widget, gpointer data);

#endif
