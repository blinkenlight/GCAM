/**
 *  gui_opengl.h
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
#ifndef _GUI_OPENGL_H
#define _GUI_OPENGL_H

#include "gcode.h"
#include "gui_define.h"
#include <GL/gl.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

#define GUI_OPENGL_CLEAR_COLOR              0.2

#define GUI_OPENGL_MAX_ZOOM                 500.0                               /* Twice the largest MAX_DIM value */
#define GUI_OPENGL_MIN_ZOOM                 0.1

#define GUI_OPENGL_MODE_EDIT                0x0
#define GUI_OPENGL_MODE_RENDER              0x1

#define GUI_OPENGL_VIEW_REGULAR             0x0
#define GUI_OPENGL_VIEW_EXTRUSION           0x1

#define GUI_OPENGL_PROJECTION_PERSPECTIVE   0x0
#define GUI_OPENGL_PROJECTION_ORTHOGRAPHIC  0x1

typedef void gui_opengl_progress_t (void *gui, gfloat_t progress);

typedef struct gui_opengl_view_s
{
  gfloat_t pos[3];
  gfloat_t azim;
  gfloat_t elev;
  gfloat_t zoom;
  gfloat_t grid;
} gui_opengl_view_t;

typedef struct gui_opengl_s
{
  uint16_t context_w;
  uint16_t context_h;

  GdkGLContext *gl_context;
  GdkGLDrawable *gl_drawable;

  uint8_t draw;
  uint8_t ready;
  uint8_t projection;
  uint8_t mode;
  uint8_t view;

  uint32_t gridxy_1_display_list;
  uint32_t gridxy_2_display_list;
  uint32_t gridxy_3_display_list;
  uint32_t gridxz_display_list;
  uint32_t simulate_display_list;

  uint32_t view_display_list;
  uint32_t rebuild_view_display_list;

  gfloat_t matx_origin;
  gfloat_t maty_origin;
  gfloat_t matz_origin;

  gui_opengl_view_t views[2];

  gcode_t *gcode;

  gui_opengl_progress_t *progress_callback;
} gui_opengl_t;

void gui_opengl_build_gridxy_display_list (gui_opengl_t *opengl);
void gui_opengl_build_gridxz_display_list (gui_opengl_t *opengl);
void gui_opengl_build_simulate_display_list (gui_opengl_t *opengl);
void gui_opengl_context_redraw (gui_opengl_t *opengl, gcode_block_t *block);

void gui_opengl_pick (gui_opengl_t *opengl, int x, int y);

void gui_opengl_context_prep (gui_opengl_t *opengl);
void gui_opengl_context_init (GtkWidget *widget, GdkEventConfigure *event, gpointer data);

#endif
