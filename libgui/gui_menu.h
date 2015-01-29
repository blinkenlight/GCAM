/**
 *  gui_menu.h
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

#ifndef _GUI_MENU_H
#define _GUI_MENU_H

#include <gtk/gtk.h>
#include "gui_menu_file.h"
#include "gui_menu_edit.h"
#include "gui_menu_insert.h"
#include "gui_menu_assistant.h"
#include "gui_menu_view.h"
#include "gui_menu_help.h"
#include "gcam_icon.h"

/* *INDENT-OFF* */

static GtkActionEntry gui_menu_entries[] = {
  { "FileMenu",                    NULL,                              "_File" },
  { "New",                         GTK_STOCK_NEW,                     "_New Project",                 "<control>N",        "Create a new GCAM Project",        G_CALLBACK (gui_menu_file_new_project_menuitem_callback) },
  { "Open",                        GTK_STOCK_OPEN,                    "_Open Project",                "<control>O",        "Open an existing GCAM Project",    G_CALLBACK (gui_menu_file_load_project_menuitem_callback) },
  { "Save",                        GTK_STOCK_SAVE,                    "_Save Project",                "<control>S",        "Save current GCAM Project",        G_CALLBACK (gui_menu_file_save_project_menuitem_callback) },
  { "Save As",                     GTK_STOCK_SAVE_AS,                 "Save Project _As",             "<control>A",        "Save current GCAM Project As",     G_CALLBACK (gui_menu_file_save_project_as_menuitem_callback) },
  { "Close",                       GTK_STOCK_CLOSE,                   "_Close Project",               "<control>W",        "Close current GCAM Project",       G_CALLBACK (gui_menu_file_close_project_menuitem_callback) },
  { "Export",                      GTK_STOCK_CONVERT,                 "_Export G-Code",               "<control>E",        "Export Project to G-Code",         G_CALLBACK (gui_menu_file_export_gcode_menuitem_callback) },
  { "Import GCAM",                 GTK_STOCK_OPEN,                    "_Import GCAM",                 "<control>I",        "Import Blocks from GCAM File",     G_CALLBACK (gui_menu_file_import_gcam_menuitem_callback) },
  { "Import Gerber (RS274X)",      GTK_STOCK_OPEN,                    "Import _Gerber (RS274X)",      "<control>G",        "Import Gerber (RS274X) to Sketch", G_CALLBACK (gui_menu_file_import_gerber_menuitem_callback) },
  { "Import Excellon Drill Holes", GTK_STOCK_OPEN,                    "Import E_xcellon Drill Holes", "<control>X",        "Import Excellon Drill Holes",      G_CALLBACK (gui_menu_file_import_excellon_menuitem_callback) },
  { "Import SVG Paths",            GTK_STOCK_OPEN,                    "Import SVG _Paths",            "<control>P",        "Import SVG Paths",                 G_CALLBACK (gui_menu_file_import_svg_menuitem_callback) },
  { "Quit",                        GTK_STOCK_QUIT,                    "_Quit",                        "<control>Q",        "Quit GCAM",                        G_CALLBACK (gui_menu_file_quit_menuitem_callback) },
  { "EditMenu",                    NULL,                              "_Edit" },
  { "Remove",                      GTK_STOCK_DELETE,                  "_Remove",                      "<control>R",        "Remove",                           G_CALLBACK (gui_menu_edit_remove_menuitem_callback) },
  { "Duplicate",                   GTK_STOCK_COPY,                    "_Duplicate",                   "<control>D",        "Duplicate",                        G_CALLBACK (gui_menu_edit_duplicate_menuitem_callback) },
  { "Translate",                   GCAM_STOCK_EDIT_TRANSLATE,         "_Translate",                   "<alt><control>T",   "Translate",                        G_CALLBACK (gui_menu_edit_move_menuitem_callback) },
  { "Rotate",                      GCAM_STOCK_EDIT_ROTATE,            "_Rotate",                      "<alt><control>R",   "Rotate",                           G_CALLBACK (gui_menu_edit_spin_menuitem_callback) },
  { "Scale",                       GCAM_STOCK_EDIT_SCALE,             "_Scale",                       "<alt><control>S",   "Scale",                            G_CALLBACK (gui_menu_edit_scale_menuitem_callback) },
  { "Attract Previous",            GCAM_STOCK_EDIT_ATTRACT_PREV,      "Attract _Previous",            "<alt><control>P",   "Attract Previous",                 G_CALLBACK (gui_menu_edit_attract_previous_menuitem_callback) },
  { "Attract Next",                GCAM_STOCK_EDIT_ATTRACT_NEXT,      "Attract _Next",                "<alt><control>N",   "Attract Next",                     G_CALLBACK (gui_menu_edit_attract_next_menuitem_callback) },
  { "Fillet Previous",             GCAM_STOCK_EDIT_FILLET_PREV,       "Fillet Previous",              NULL,                "Fillet Previous",                  G_CALLBACK (gui_menu_edit_fillet_previous_menuitem_callback) },
  { "Fillet Next",                 GCAM_STOCK_EDIT_FILLET_NEXT,       "Fillet Next",                  NULL,                "Fillet Next",                      G_CALLBACK (gui_menu_edit_fillet_next_menuitem_callback) },
  { "Flip Direction",              GCAM_STOCK_EDIT_FLIP_DIRECTION,    "F_lip Direction",              NULL,                "Flip Direction",                   G_CALLBACK (gui_menu_edit_flip_direction_menuitem_callback) },
  { "Optimize Order",              GCAM_STOCK_EDIT_OPTIMIZE_ORDER,    "_Optimize Order",              NULL,                "Optimize Order",                   G_CALLBACK (gui_menu_edit_optimize_order_menuitem_callback) },
  { "Generate Pattern",            GCAM_STOCK_EDIT_GENERATE_PATTERN,  "_Generate Pattern",            NULL,                "Generate Pattern",                 G_CALLBACK (gui_menu_edit_generate_pattern_menuitem_callback) },
  { "Project Settings",            GTK_STOCK_PROPERTIES,              "_Project Settings",            "<control>P",        "Project Settings",                 G_CALLBACK (gui_menu_edit_project_settings_menuitem_callback) },
  { "InsertMenu",                  NULL,                              "_Insert" },
  { "Tool Change",                 GCAM_STOCK_INSERT_TOOL,            "Tool Change",                  "<control><shift>C", "Insert Tool Change",               G_CALLBACK (gui_menu_insert_tool_change_menuitem_callback) },
  { "Template",                    GCAM_STOCK_INSERT_TEMPLATE,        "Template",                     "<control><shift>T", "Insert Template",                  G_CALLBACK (gui_menu_insert_template_menuitem_callback) },
  { "Sketch",                      GCAM_STOCK_INSERT_SKETCH,          "Sketch",                       "<control><shift>S", "Insert Sketch",                    G_CALLBACK (gui_menu_insert_sketch_menuitem_callback) },
  { "Arc",                         GCAM_STOCK_INSERT_ARC,             "Arc",                          "<control><shift>A", "Insert Arc",                       G_CALLBACK (gui_menu_insert_arc_menuitem_callback) },
  { "Line",                        GCAM_STOCK_INSERT_LINE,            "Line",                         "<control><shift>L", "Insert Line",                      G_CALLBACK (gui_menu_insert_line_menuitem_callback) },
  { "Bolt Holes",                  GCAM_STOCK_INSERT_BOLT_HOLES,      "Bolt Holes",                   "<control><shift>B", "Insert Bolt Holes",                G_CALLBACK (gui_menu_insert_bolt_holes_menuitem_callback) },
  { "Drill Holes",                 GCAM_STOCK_INSERT_DRILL_HOLES,     "Drill Holes",                  "<control><shift>D", "Insert Drill Holes",               G_CALLBACK (gui_menu_insert_drill_holes_menuitem_callback) },
  { "Point",                       GCAM_STOCK_INSERT_POINT,           "Point",                        "<control><shift>P", "Insert Point",                     G_CALLBACK (gui_menu_insert_point_menuitem_callback) },
  { "Image",                       GCAM_STOCK_INSERT_IMAGE,           "Image",                        "<control><shift>I", "Insert Image",                     G_CALLBACK (gui_menu_insert_image_menuitem_callback) },
  { "AssistantMenu",               NULL,                              "_Assistant" },
  { "Polygon",                     GCAM_STOCK_ASSIST_POLYGON,         "_Polygon",                     NULL,                "Create Polygon",                   G_CALLBACK (gui_menu_assistant_polygon_menuitem_callback) },
  { "ViewMenu",                    NULL,                              "_View" },
  { "Perspective",                 GCAM_STOCK_VIEW_PERSPECTIVE,       "_Perspective",                 NULL,                "View Perspective",                 G_CALLBACK (gui_menu_view_perspective_menuitem_callback) },
  { "Orthographic",                GCAM_STOCK_VIEW_ORTHOGRAPHIC,      "_Orthographic",                NULL,                "View Orthographic",                G_CALLBACK (gui_menu_view_orthographic_menuitem_callback) },
  { "Top",                         GCAM_STOCK_VIEW_TOP,               "_Top",                         NULL,                "View Top",                         G_CALLBACK (gui_menu_view_top_menuitem_callback) },
  { "Left",                        GCAM_STOCK_VIEW_LEFT,              "_Left",                        NULL,                "View Left",                        G_CALLBACK (gui_menu_view_left_menuitem_callback) },
  { "Right",                       GCAM_STOCK_VIEW_RIGHT,             "_Right",                       NULL,                "View Right",                       G_CALLBACK (gui_menu_view_right_menuitem_callback) },
  { "Front",                       GCAM_STOCK_VIEW_FRONT,             "_Front",                       NULL,                "View Front",                       G_CALLBACK (gui_menu_view_front_menuitem_callback) },
  { "Back",                        GCAM_STOCK_VIEW_BACK,              "_Back",                        NULL,                "View Back",                        G_CALLBACK (gui_menu_view_back_menuitem_callback) },
  { "RenderMenu",                  NULL,                              "_Render" },
  { "FinalPart",                   GTK_STOCK_EXECUTE,                 "_Final Part",                  "<control>F",        "Render Final Part",                G_CALLBACK (gui_menu_view_render_final_part_menuitem_callback) },
  { "HelpMenu",                    NULL,                              "_Help" },
  { "Manual",                      GTK_STOCK_HELP,                    "_Manual",                      NULL,                "GCAM Manual",                      G_CALLBACK (gui_menu_help_manual_menuitem_callback) },
  { "About",                       GTK_STOCK_ABOUT,                   "_About",                       NULL,                "About GCAM",                       G_CALLBACK (gui_menu_help_about_menuitem_callback) },
};

static const char *gui_menu_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='New'/>"
"      <menuitem action='Open'/>"
"      <menuitem action='Save'/>"
"      <menuitem action='Save As'/>"
"      <menuitem action='Close'/>"
"      <separator/>"
"      <menuitem action='Export'/>"
"      <separator/>"
"      <menuitem action='Import GCAM'/>"
"      <menuitem action='Import Gerber (RS274X)'/>"
"      <menuitem action='Import Excellon Drill Holes'/>"
"      <menuitem action='Import SVG Paths'/>"
"      <separator/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='EditMenu'>"
"      <menuitem action='Remove'/>"
"      <menuitem action='Duplicate'/>"
"      <separator/>"
"      <menuitem action='Translate'/>"
"      <menuitem action='Rotate'/>"
"      <menuitem action='Scale'/>"
"      <separator/>"
"      <menuitem action='Attract Previous'/>"
"      <menuitem action='Attract Next'/>"
"      <separator/>"
"      <menuitem action='Fillet Previous'/>"
"      <menuitem action='Fillet Next'/>"
"      <separator/>"
"      <menuitem action='Flip Direction'/>"
"      <separator/>"
"      <menuitem action='Optimize Order'/>"
"      <separator/>"
"      <menuitem action='Generate Pattern'/>"
"      <separator/>"
"      <menuitem action='Project Settings'/>"
"    </menu>"
"    <menu action='InsertMenu'>"
"      <menuitem action='Tool Change'/>"
"      <separator/>"
"      <menuitem action='Template'/>"
"      <separator/>"
"      <menuitem action='Sketch'/>"
"      <menuitem action='Arc'/>"
"      <menuitem action='Line'/>"
"      <separator/>"
"      <menuitem action='Bolt Holes'/>"
"      <separator/>"
"      <menuitem action='Drill Holes'/>"
"      <menuitem action='Point'/>"
"      <separator/>"
"      <menuitem action='Image'/>"
"    </menu>"
"    <menu action='AssistantMenu'>"
"      <menuitem action='Polygon'/>"
"    </menu>"
"    <menu action='ViewMenu'>"
"      <menuitem action='Perspective'/>"
"      <menuitem action='Orthographic'/>"
"      <separator/>"
"      <menuitem action='Top'/>"
"      <menuitem action='Left'/>"
"      <menuitem action='Right'/>"
"      <menuitem action='Front'/>"
"      <menuitem action='Back'/>"
"    </menu>"
"    <menu action='RenderMenu'>"
"      <menuitem action='FinalPart'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='Manual'/>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"</ui>";

/* *INDENT-ON* */

#endif
