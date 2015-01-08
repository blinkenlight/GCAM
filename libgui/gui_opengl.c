/**
 *  gui_opengl.c
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

#include "gui_opengl.h"
#include "gui.h"
#include "gui_tab.h"
#include "gui_menu_util.h"
#include <GL/glu.h>

static void
sum_normal (gui_opengl_t *opengl, int i, int j, int k, gcode_vec3d_t nor)
{
  int ind;

  if (i < 0 || j < 0 || k < 0 || i >= opengl->gcode->voxel_number[0] || j >= opengl->gcode->voxel_number[1] || k >= opengl->gcode->voxel_number[2])
    return;

  ind = i + j * opengl->gcode->voxel_number[0] + k * (opengl->gcode->voxel_number[0] * opengl->gcode->voxel_number[1]);

  /**
   * Compute Normal Based on Existance of Neighbors
   * -x = -1
   * +x = +1
   * -y = -opengl->gcode->voxel_number[1]
   * +y = +opengl->gcode->voxel_number[1]
   * -z = -(opengl->gcode->voxel_number[0] * opengl->gcode->voxel_number[1])
   * +z = +(opengl->gcode->voxel_number[0] * opengl->gcode->voxel_number[1])
   */

  /* X */
  if (i > 0)
  {
    if (!opengl->gcode->voxel_map[ind - 1])
      nor[0] += -1.0;
  }
  else
  {
    nor[0] += -1.0;
  }

  if (i < opengl->gcode->voxel_number[0] - 1)
  {
    if (!opengl->gcode->voxel_map[ind + 1])
      nor[0] += 1.0;
  }
  else
  {
    nor[0] += 1.0;
  }

  /* Y */
  if (j > 0)
  {
    if (!opengl->gcode->voxel_map[ind - opengl->gcode->voxel_number[0]])
      nor[1] += -1.0;
  }
  else
  {
    nor[1] += -1.0;
  }

  if (j < opengl->gcode->voxel_number[1] - 1)
  {
    if (!opengl->gcode->voxel_map[ind + opengl->gcode->voxel_number[0]])
      nor[1] += 1.0;
  }
  else
  {
    nor[1] += 1.0;
  }

  /* Z */
  if (k > 0)
  {
    if (!opengl->gcode->voxel_map[ind - opengl->gcode->voxel_number[0] * opengl->gcode->voxel_number[1]])
      nor[2] += -1.0;
  }
  else
  {
    nor[2] += -1.0;
  }

  if (k < opengl->gcode->voxel_number[2] - 1)
  {
    if (!opengl->gcode->voxel_map[ind + opengl->gcode->voxel_number[0] * opengl->gcode->voxel_number[1]])
      nor[2] += 1.0;
  }
  else
  {
    nor[2] += 1.0;
  }
}

/**
 * Build the opengl lists containing the three XY grids (fine / medium / coarse)
 * including the borders and axes involved; notably though, this is not actually 
 * drawing them - it's only building the lists that will draw them when called;
 */

void
gui_opengl_build_gridxy_display_list (gui_opengl_t *opengl)
{
  gfloat_t i, incr[3];

  opengl->matx_origin = -opengl->gcode->material_size[0] * 0.5;
  opengl->maty_origin = -opengl->gcode->material_size[1] * 0.5;
  opengl->matz_origin = -opengl->gcode->material_size[2] * 0.5;

  if (opengl->gcode->units == GCODE_UNITS_INCH)
  {
    if (opengl->gcode->material_size[0] >= 12.0 || opengl->gcode->material_size[1] >= 12.0)
    {
      incr[0] = 12.0;
      incr[1] = 1.0;
      incr[2] = 0.1;
    }
    else
    {
      incr[0] = 1.0;
      incr[1] = 0.1;
      incr[2] = 0.01;
    }
  }
  else
  {
    if (opengl->gcode->material_size[0] >= 1000.0 || opengl->gcode->material_size[1] >= 1000.0)
    {
      incr[0] = 100.0;
      incr[1] = 10.0;
      incr[2] = 1.0;
    }
    else
    {
      incr[0] = 10.0;
      incr[1] = 1.0;
      incr[2] = 0.1;
    }
  }

  opengl->gridxy_1_display_list = glGenLists (1);
  glNewList (opengl->gridxy_1_display_list, GL_COMPILE);

  glLineWidth (1);

  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_COARSE_GRID_COLOR[0],
             GCODE_OPENGL_COARSE_GRID_COLOR[1],
             GCODE_OPENGL_COARSE_GRID_COLOR[2]);

  /* Lines spanning X axis */
  for (i = 0.0; i < opengl->gcode->material_size[0]; i += incr[0])
  {
    glVertex3f (opengl->matx_origin + i, opengl->maty_origin, 0.0);
    glVertex3f (opengl->matx_origin + i, opengl->maty_origin + opengl->gcode->material_size[1], 0.0);
  }

  /* Lines spanning Y axis */
  for (i = 0.0; i < opengl->gcode->material_size[1]; i += incr[0])
  {
    glVertex3f (opengl->matx_origin, opengl->maty_origin + i, 0.0);
    glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + i, 0.0);
  }

  glEnd ();

  /* Borders at Zero */
  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_GRID_BORDER_COLOR[0],
             GCODE_OPENGL_GRID_BORDER_COLOR[1],
             GCODE_OPENGL_GRID_BORDER_COLOR[2]);

  glVertex3f (opengl->matx_origin, opengl->maty_origin, 0.0);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin, 0.0);

  glVertex3f (opengl->matx_origin, opengl->maty_origin + opengl->gcode->material_size[1], 0.0);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + opengl->gcode->material_size[1], 0.0);

  glVertex3f (opengl->matx_origin, opengl->maty_origin, 0.0);
  glVertex3f (opengl->matx_origin, opengl->maty_origin + opengl->gcode->material_size[1], 0.0);

  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin, 0.0);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + opengl->gcode->material_size[1], 0.0);
  glEnd ();

  /* Borders at Depth */
  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_GRID_BORDER_COLOR[0],
             GCODE_OPENGL_GRID_BORDER_COLOR[1],
             GCODE_OPENGL_GRID_BORDER_COLOR[2]);

  glVertex3f (opengl->matx_origin, opengl->maty_origin, -opengl->gcode->material_size[2]);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin, -opengl->gcode->material_size[2]);

  glVertex3f (opengl->matx_origin, opengl->maty_origin + opengl->gcode->material_size[1], -opengl->gcode->material_size[2]);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + opengl->gcode->material_size[1], -opengl->gcode->material_size[2]);

  glVertex3f (opengl->matx_origin, opengl->maty_origin, -opengl->gcode->material_size[2]);
  glVertex3f (opengl->matx_origin, opengl->maty_origin + opengl->gcode->material_size[1], -opengl->gcode->material_size[2]);

  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin, -opengl->gcode->material_size[2]);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + opengl->gcode->material_size[1], -opengl->gcode->material_size[2]);
  glEnd ();

  /* Borders at Origin */
  glLineWidth (2);

  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_GRID_ORIGIN_COLOR[0],
             GCODE_OPENGL_GRID_ORIGIN_COLOR[1],
             GCODE_OPENGL_GRID_ORIGIN_COLOR[2]);

  glVertex3f (opengl->matx_origin, opengl->maty_origin + opengl->gcode->material_origin[1], 0.0);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + opengl->gcode->material_origin[1], 0.0);

  glVertex3f (opengl->matx_origin + opengl->gcode->material_origin[0], opengl->maty_origin, 0.0);
  glVertex3f (opengl->matx_origin + opengl->gcode->material_origin[0], opengl->maty_origin + opengl->gcode->material_size[1], 0.0);
  glEnd ();

  glLineWidth (1);

  glEndList ();

  opengl->gridxy_2_display_list = glGenLists (2);
  glNewList (opengl->gridxy_2_display_list, GL_COMPILE);

  glLineWidth (1);

  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_MEDIUM_GRID_COLOR[0],
             GCODE_OPENGL_MEDIUM_GRID_COLOR[1],
             GCODE_OPENGL_MEDIUM_GRID_COLOR[2]);

  /* Lines spanning X axis */
  for (i = 0.0; i < opengl->gcode->material_size[0]; i += incr[1])
  {
    glVertex3f (opengl->matx_origin + i, opengl->maty_origin, 0.0);
    glVertex3f (opengl->matx_origin + i, opengl->maty_origin + opengl->gcode->material_size[1], 0.0);
  }

  /* Lines spanning Y axis */
  for (i = 0.0; i < opengl->gcode->material_size[1]; i += incr[1])
  {
    glVertex3f (opengl->matx_origin, opengl->maty_origin + i, 0.0);
    glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + i, 0.0);
  }

  glEnd ();

  glEndList ();

  opengl->gridxy_3_display_list = glGenLists (3);
  glNewList (opengl->gridxy_3_display_list, GL_COMPILE);

  glLineWidth (1);

  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_FINEST_GRID_COLOR[0],
             GCODE_OPENGL_FINEST_GRID_COLOR[1],
             GCODE_OPENGL_FINEST_GRID_COLOR[2]);

  /* Lines spanning X axis */
  for (i = 0.0; i < opengl->gcode->material_size[0]; i += incr[2])
  {
    glVertex3f (opengl->matx_origin + i, opengl->maty_origin, 0.0);
    glVertex3f (opengl->matx_origin + i, opengl->maty_origin + opengl->gcode->material_size[1], 0.0);
  }

  /* Lines spanning Y axis */
  for (i = 0.0; i < opengl->gcode->material_size[1]; i += incr[2])
  {
    glVertex3f (opengl->matx_origin, opengl->maty_origin + i, 0.0);
    glVertex3f (opengl->matx_origin + opengl->gcode->material_size[0], opengl->maty_origin + i, 0.0);
  }

  glEnd ();

  glEndList ();
}

/**
 * Build the opengl list containing the XZ grid including the borders and axes 
 * involved; notably though, this is not actually drawing them - it's only 
 * building the lists that will draw them when called;
 */

void
gui_opengl_build_gridxz_display_list (gui_opengl_t *opengl)
{
  gfloat_t i, incr;

  if (opengl->gcode->units == GCODE_UNITS_INCH)
    incr = 0.1;
  else
    incr = 1;

  opengl->gridxz_display_list = glGenLists (4);
  glNewList (opengl->gridxz_display_list, GL_COMPILE);

  glDisable (GL_LIGHTING);
  glLineWidth (1);

  /* Lines spanning X axis (vertical lines) */
  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_FINEST_GRID_COLOR[0],
             GCODE_OPENGL_FINEST_GRID_COLOR[1],
             GCODE_OPENGL_FINEST_GRID_COLOR[2]);

  for (i = 0; i < opengl->gcode->material_size[0] * 0.5; i += incr)
  {
    glVertex3f (i, 0.0, 0.0);
    glVertex3f (i, -opengl->gcode->material_size[2], 0.0);
  }

  glEnd ();

  /* Lines spanning Z axis (horizontal lines) */
  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_FINEST_GRID_COLOR[0],
             GCODE_OPENGL_FINEST_GRID_COLOR[1],
             GCODE_OPENGL_FINEST_GRID_COLOR[2]);

  for (i = 0; i < opengl->gcode->material_size[2]; i += incr)
  {
    glVertex3f (0.0, -i, 0.0);
    glVertex3f (opengl->gcode->material_size[0] * 0.5, -i, 0.0);
  }

  glEnd ();

  /* Borders */
  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_GRID_BORDER_COLOR[0],
             GCODE_OPENGL_GRID_BORDER_COLOR[1],
             GCODE_OPENGL_GRID_BORDER_COLOR[2]);

  glVertex3f (0.0, 0.0, 0.0);
  glVertex3f (opengl->gcode->material_size[0] * 0.5, 0.0, 0.0);

  glVertex3f (0.0, -opengl->gcode->material_size[2], 0.0);
  glVertex3f (opengl->gcode->material_size[0] * 0.5, -opengl->gcode->material_size[2], 0.0);
  glEnd ();

  /* Zero Line */
  glLineWidth (2);

  glBegin (GL_LINES);
  glColor3f (GCODE_OPENGL_GRID_ORIGIN_COLOR[0],
             GCODE_OPENGL_GRID_ORIGIN_COLOR[1],
             GCODE_OPENGL_GRID_ORIGIN_COLOR[2]);

  glVertex3f (0.0, 0.0, 0.0);
  glVertex3f (0.0, -opengl->gcode->material_size[2], 0.0);
  glEnd ();

  glLineWidth (1);

  glEndList ();
}

void
gui_opengl_build_simulate_display_list (gui_opengl_t *opengl)
{
  int16_t i, j, k;
  gfloat_t vx, vy, vz;
  gcode_vec3d_t nor;
  int ind;
  GLfloat mat_ambient[] = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat mat_diffuse[] = { 0.6, 0.6, 0.6, 1.0 };
  GLfloat mat_specular[] = { 0.0, 0.0, 0.0, 1.0 };
  GLfloat mat_shininess[] = { 0.0 };

  if (!opengl->gcode->voxel_map)
    return;

  opengl->simulate_display_list = glGenLists (5);
  glNewList (opengl->simulate_display_list, GL_COMPILE);

  glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

  glEnable (GL_LIGHTING);
  glEnable (GL_LIGHT0);

  glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
  glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
  glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

  glPointSize (GCODE_OPENGL_VOXEL_POINT_SIZE);
  glBegin (GL_POINTS);

  ind = 0;

  for (k = 0; k < opengl->gcode->voxel_number[2]; k++)
  {
    vz = ((gfloat_t)k / (gfloat_t)opengl->gcode->voxel_number[2]) * opengl->gcode->material_size[2] - opengl->gcode->material_size[2];

    /* Update Progress based on Z for now */
    opengl->progress_callback (opengl->gcode->gui, (gfloat_t)(k + 1) / (gfloat_t)opengl->gcode->voxel_number[2]);

    for (j = 0; j < opengl->gcode->voxel_number[1]; j++)
    {
      vy = -opengl->gcode->material_size[1] * 0.5 + ((gfloat_t)j / (gfloat_t)opengl->gcode->voxel_number[1]) * opengl->gcode->material_size[1];

      for (i = 0; i < opengl->gcode->voxel_number[0]; i++)
      {
        vx = -opengl->gcode->material_size[0] * 0.5 + ((gfloat_t)i / (gfloat_t)opengl->gcode->voxel_number[0]) * opengl->gcode->material_size[0];

        if (opengl->gcode->voxel_map[ind])
        {
          nor[0] = 0.0;
          nor[1] = 0.0;
          nor[2] = 0.0;

          sum_normal (opengl, i, j, k, nor);

          sum_normal (opengl, i - 1, j, k, nor);
          sum_normal (opengl, i + 1, j, k, nor);
          sum_normal (opengl, i, j - 1, k, nor);
          sum_normal (opengl, i, j + 1, k, nor);
          sum_normal (opengl, i, j, k - 1, nor);
          sum_normal (opengl, i, j, k + 1, nor);

          if (fabs (nor[0]) + fabs (nor[1]) + fabs (nor[2]) > 0.0)
          {
            glNormal3f (nor[0], nor[1], nor[2]);
            glVertex3f (vx, vy, vz);
          }
        }

        ind++;
      }
    }
  }

  glEnd ();

  glEndList ();
}

/**
 * If the opengl struct member 'rebuild_view_display_list' is TRUE, rebuild the
 * opengl list containing the graphic representation of the gcode block tree by
 * looping through all the top level blocks and calling 'draw' for each; either
 * way, once the list exists, call it thereby effectively redrawing all blocks;
 */

static void
draw_top_level_blocks (gui_opengl_t *opengl, gcode_block_t *selected_block)
{
  gcode_block_t *block;

  glEnable (GL_DEPTH_TEST);

  if (opengl->rebuild_view_display_list)
  {
    if (opengl->view_display_list)
      glDeleteLists (opengl->view_display_list, 1);

    opengl->view_display_list = glGenLists (6);
    glNewList (opengl->view_display_list, GL_COMPILE);

    glTranslatef (opengl->matx_origin + opengl->gcode->material_origin[0], opengl->maty_origin + opengl->gcode->material_origin[1], 0.0);

    block = opengl->gcode->listhead;

    while (block)
    {
      if (block->draw)
        block->draw (block, selected_block);

      block = block->next;
    }

    glTranslatef (-opengl->matx_origin - opengl->gcode->material_origin[0], -opengl->maty_origin - opengl->gcode->material_origin[1], 0.0);
    glEndList ();

    opengl->rebuild_view_display_list = 0;
  }

  glCallList (opengl->view_display_list);
}

/**
 * Set up the appropriate projection (orthogonal or perspective, depending on
 * the value of the 'projection' opengl struct member) for the specified view
 * then update the 'view' opengl struct member accordingly;
 */

static void
set_projection (gui_opengl_t *opengl, uint8_t view)
{
  gfloat_t aspect, fov;

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  opengl->view = view;

  aspect = (gfloat_t)opengl->context_h / (gfloat_t)opengl->context_w;

  if ((view == GUI_OPENGL_VIEW_REGULAR) && (opengl->projection == GUI_OPENGL_PROJECTION_PERSPECTIVE))
  {
    fov = GCODE_UNITS (opengl->gcode, GUI_OPENGL_MIN_ZOOM) * 12.5 * GCODE_PI / 180.0;

    glFrustum (-tan (fov),
               tan (fov),
               -aspect * tan (fov),
               aspect * tan (fov),
               GCODE_UNITS (opengl->gcode, GUI_OPENGL_MIN_ZOOM) * 0.2,
               GCODE_UNITS (opengl->gcode, GUI_OPENGL_MAX_ZOOM) * 0.2);
  }
  else
  {
    glOrtho (-opengl->views[view].grid,
             opengl->views[view].grid,
             -opengl->views[view].grid * aspect,
             opengl->views[view].grid * aspect,
             -GCODE_UNITS (opengl->gcode, GUI_OPENGL_MIN_ZOOM) * 5.0,
             GCODE_UNITS (opengl->gcode, GUI_OPENGL_MAX_ZOOM) * 5.0);
  }

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
}

/**
 * Draw the three XY grids as needed depending on zoom level and material size;
 */

static void
draw_XY_grid (gui_opengl_t *opengl)
{
  gfloat_t coef, zoom;

  glDisable (GL_DEPTH_TEST);

  if (opengl->projection == GUI_OPENGL_PROJECTION_PERSPECTIVE)
    zoom = 0.15 * opengl->views[GUI_OPENGL_VIEW_REGULAR].zoom;
  else
    zoom = 0.15 * opengl->views[GUI_OPENGL_VIEW_REGULAR].grid;

  if (opengl->gcode->units == GCODE_UNITS_INCH)
  {
    if (opengl->gcode->material_size[0] >= 12.0 || opengl->gcode->material_size[1] >= 12.0)
      zoom *= 0.1;
  }
  else
  {
    if (opengl->gcode->material_size[0] >= 1000.0 || opengl->gcode->material_size[1] >= 1000.0)
      zoom *= 0.1;
  }

  /* Fine Grid */
  if (opengl->gcode->units == GCODE_UNITS_INCH)
    coef = 0.04 / zoom;
  else
    coef = 0.4 / zoom;

  if (coef > 0.5)
    glCallList (opengl->gridxy_3_display_list);

  /* Medium Grid */
  if (opengl->gcode->units == GCODE_UNITS_INCH)
    coef = 0.2 / zoom;
  else
    coef = 2.0 / zoom;

  if (coef > 0.5)
    glCallList (opengl->gridxy_2_display_list);

  /* Coarse Grid */
  glCallList (opengl->gridxy_1_display_list);
}

/**
 * Draw the XZ grid;
 */

static void
draw_XZ_grid (gui_opengl_t *opengl)
{
  glDisable (GL_DEPTH_TEST);

  glCallList (opengl->gridxz_display_list);
}

/**
 * Draw the appropriate view (regular or extrusion) depending on the currently
 * selected block type by setting up the appropriate projection (orthogonal or
 * perspective), drawing the appropriate grid (XY or XZ) and calling the 'draw'
 * method of the appropriate blocks (the extrusion or all the top level blocks)
 */

static void
draw_all (gui_opengl_t *opengl, gcode_block_t *block)
{
  gcode_block_t *extr_block;

  int view = GUI_OPENGL_VIEW_REGULAR;

  if (block->type == GCODE_TYPE_EXTRUSION)
  {
    extr_block = block;
    view = GUI_OPENGL_VIEW_EXTRUSION;
  }

  if (block->parent)
  {
    if (block->parent->type == GCODE_TYPE_EXTRUSION)
    {
      extr_block = block->parent;
      view = GUI_OPENGL_VIEW_EXTRUSION;
    }
  }

  set_projection (opengl, view);

  glTranslatef (0, 0, -opengl->views[view].zoom);
  glRotatef (opengl->views[view].elev - 90.0, 1, 0, 0);
  glRotatef (opengl->views[view].azim, 0, 0, 1);
  glTranslatef (-opengl->views[view].pos[0], -opengl->views[view].pos[1], -opengl->views[view].pos[2]);

  if (view == GUI_OPENGL_VIEW_EXTRUSION)
  {
    draw_XZ_grid (opengl);
    extr_block->draw (extr_block, block);
  }
  else
  {
    draw_XY_grid (opengl);
    draw_top_level_blocks (opengl, block);                                      // New behaviour, show everything now that suppress exists
  }
}

/**
 * Draw the entire graphic viewport from the background up, either in one of the
 * normal modes ('regular' or 'extrusion' editing) or in render mode voxel view;
 * Essentially, this is the most global opengl repaint request the code can call
 */

void
gui_opengl_context_redraw (gui_opengl_t *opengl, gcode_block_t *block)
{
  uint8_t view;

  view = GUI_OPENGL_VIEW_REGULAR;

  gdk_gl_drawable_gl_begin (opengl->gl_drawable, opengl->gl_context);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  glOrtho (-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  /* Draw Background Gradient */
  glDisable (GL_LIGHTING);
  glBegin (GL_QUADS);
  glColor3f (GCODE_OPENGL_BACKGROUND_COLORS[0][0],
             GCODE_OPENGL_BACKGROUND_COLORS[0][1],
             GCODE_OPENGL_BACKGROUND_COLORS[0][2]);
  glVertex3f (-1.0, -1.0, 0.0);

  glColor3f (GCODE_OPENGL_BACKGROUND_COLORS[1][0],
             GCODE_OPENGL_BACKGROUND_COLORS[1][1],
             GCODE_OPENGL_BACKGROUND_COLORS[1][2]);
  glVertex3f (1.0, -1.0, 0.0);

  glColor3f (GCODE_OPENGL_BACKGROUND_COLORS[2][0],
             GCODE_OPENGL_BACKGROUND_COLORS[2][1],
             GCODE_OPENGL_BACKGROUND_COLORS[2][2]);
  glVertex3f (1.0, 1.0, 0.0);

  glColor3f (GCODE_OPENGL_BACKGROUND_COLORS[3][0],
             GCODE_OPENGL_BACKGROUND_COLORS[3][1],
             GCODE_OPENGL_BACKGROUND_COLORS[3][2]);
  glVertex3f (-1.0, 1.0, 0.0);
  glEnd ();

  set_projection (opengl, view);

  /* Make sure the block is non NULL and has a drawing function */
  if (opengl->ready)
  {
    switch (opengl->mode)
    {
      case GUI_OPENGL_MODE_EDIT:
        glDisable (GL_LIGHTING);

        if (block)
        {
          if (block->draw)
          {
            draw_all (opengl, block);
          }
          else
          {
            glTranslatef (0, 0, -opengl->views[view].zoom);
            glRotatef (opengl->views[view].elev - 90.0, 1, 0, 0);
            glRotatef (opengl->views[view].azim, 0, 0, 1);
            glTranslatef (-opengl->views[view].pos[0], -opengl->views[view].pos[1], -opengl->views[view].pos[2]);

            draw_XY_grid (opengl);
            draw_top_level_blocks (opengl, block->parent);
          }
        }

        break;

      case GUI_OPENGL_MODE_RENDER:
      {
        int size;

        glTranslatef (0, 0, -opengl->views[view].zoom);
        glRotatef (opengl->views[view].elev - 90.0, 1, 0, 0);
        glRotatef (opengl->views[view].azim, 0, 0, 1);
        glTranslatef (-opengl->views[view].pos[0], -opengl->views[view].pos[1], -opengl->views[view].pos[2]);

        glEnable (GL_DEPTH_TEST);

        if (opengl->projection == GUI_OPENGL_PROJECTION_PERSPECTIVE)
          size = (int)((opengl->context_w / 500.0) * 5.0 / opengl->views[view].zoom);
        else
          size = (int)((opengl->context_w / 500.0) * 5.0 / opengl->views[view].grid);

        glCallList (opengl->simulate_display_list);

        break;
      }

      default:
        break;
    }
  }

  gdk_gl_drawable_swap_buffers (opengl->gl_drawable);
  gdk_gl_drawable_gl_end (opengl->gl_drawable);
}

void
gui_opengl_pick (gui_opengl_t *opengl, int x, int y)
{
  gfloat_t aspect, fov;
  GLuint buff[64];
  int viewport[4], hits;
  uint8_t view;

  view = GUI_OPENGL_VIEW_REGULAR;

  glSelectBuffer (64, buff);

  glGetIntegerv (GL_VIEWPORT, viewport);

  glRenderMode (GL_SELECT);

  glInitNames ();

  glPushName (0);

  /* View Configuration and Draw */
  glMatrixMode (GL_PROJECTION);

  glPushMatrix ();

  glLoadIdentity ();

  gluPickMatrix (x, viewport[3] - y, 15.0, 15.0, viewport);

  aspect = (gfloat_t)opengl->context_h / (gfloat_t)opengl->context_w;

  if (opengl->projection == GUI_OPENGL_PROJECTION_PERSPECTIVE)
  {
    fov = GCODE_UNITS (opengl->gcode, GUI_OPENGL_MIN_ZOOM) * 12.5 * GCODE_PI / 180.0;

    glFrustum (-tan (fov),
               tan (fov),
               -aspect * tan (fov),
               aspect * tan (fov),
               GCODE_UNITS (opengl->gcode, GUI_OPENGL_MIN_ZOOM) * 0.2,
               GCODE_UNITS (opengl->gcode, GUI_OPENGL_MAX_ZOOM) * 0.2);
  }
  else
  {
    glOrtho (-opengl->views[view].grid,
             opengl->views[view].grid,
             -opengl->views[view].grid * aspect,
             opengl->views[view].grid * aspect,
             -GCODE_UNITS (opengl->gcode, GUI_OPENGL_MIN_ZOOM) * 5.0,
             GCODE_UNITS (opengl->gcode, GUI_OPENGL_MAX_ZOOM) * 5.0);
  }

  glMatrixMode (GL_MODELVIEW);

  glLoadIdentity ();

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glTranslatef (0, 0, -opengl->views[view].zoom);
  glRotatef (opengl->views[view].elev - 90.0, 1, 0, 0);
  glRotatef (opengl->views[view].azim, 0, 0, 1);
  glTranslatef (-opengl->views[view].pos[0], -opengl->views[view].pos[1], -opengl->views[view].pos[2]);

  draw_top_level_blocks (opengl, NULL);

  glPopMatrix ();

  hits = glRenderMode (GL_RENDER);

  /**
   * For now, take the first hit and search the entire block treeview for the block ptr whose value
   * matches the name value from the first hit in the list.  Once located, highlight that row.
   */
  if (hits > 0)
  {
    gcode_block_t *ptr;

    ptr = (gcode_block_t *)((((intptr_t)buff[3]) << 3) + (intptr_t)opengl->gcode);

    set_selected_row_with_block (opengl->gcode->gui, ptr);
    gui_tab_display (opengl->gcode->gui, ptr, 0);
  }
}

/**
 * Initialize both regular (main) and extrusion views to defaults related to the
 * current material size and rebuild the XY and the XZ grids (as opengl lists);
 */

void
gui_opengl_context_prep (gui_opengl_t *opengl)
{
  opengl->projection = GUI_OPENGL_PROJECTION_ORTHOGRAPHIC;

  /* Set the default regular orthographic view */
  opengl->views[GUI_OPENGL_VIEW_REGULAR].pos[0] = 0.0;
  opengl->views[GUI_OPENGL_VIEW_REGULAR].pos[1] = 0.0;
  opengl->views[GUI_OPENGL_VIEW_REGULAR].pos[2] = 0.0;

  opengl->views[GUI_OPENGL_VIEW_REGULAR].elev = 90;
  opengl->views[GUI_OPENGL_VIEW_REGULAR].azim = 0;

  opengl->views[GUI_OPENGL_VIEW_REGULAR].zoom = opengl->gcode->material_size[0] * 0.66 * (gfloat_t)opengl->context_w / (gfloat_t)opengl->context_h;
  opengl->views[GUI_OPENGL_VIEW_REGULAR].grid = opengl->views[GUI_OPENGL_VIEW_REGULAR].zoom;

  /* Set the default extrusion orthographic view */
  opengl->views[GUI_OPENGL_VIEW_EXTRUSION].pos[0] = opengl->gcode->material_size[0] * 0.25;
  opengl->views[GUI_OPENGL_VIEW_EXTRUSION].pos[1] = -opengl->gcode->material_size[2] * 0.5;
  opengl->views[GUI_OPENGL_VIEW_EXTRUSION].pos[2] = 0.0;

  opengl->views[GUI_OPENGL_VIEW_EXTRUSION].elev = 90;
  opengl->views[GUI_OPENGL_VIEW_EXTRUSION].azim = 0;

  opengl->views[GUI_OPENGL_VIEW_EXTRUSION].grid = (opengl->gcode->material_size[0] > opengl->gcode->material_size[2] ? 0.5 * opengl->gcode->material_size[0] : opengl->gcode->material_size[2]);
  opengl->views[GUI_OPENGL_VIEW_EXTRUSION].zoom = opengl->views[GUI_OPENGL_VIEW_EXTRUSION].grid;

  gui_opengl_build_gridxy_display_list (opengl);
  gui_opengl_build_gridxz_display_list (opengl);
}

/**
 * Initialize the opengl viewport and store the opengl window size for further
 * reference in the struct members 'opengl.context_w' and 'opengl.context_h';
 * NOTE: This is a callback for the opengl "configure_event" which fires both on
 * startup and on any subsequent user-initiated size changes of the GUI window
 */

void
gui_opengl_context_init (GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
  gui_opengl_t *opengl;

  opengl = (gui_opengl_t *)data;

  opengl->context_w = (uint16_t)widget->allocation.width;
  opengl->context_h = (uint16_t)widget->allocation.height;

  opengl->gl_context = gtk_widget_get_gl_context (widget);
  opengl->gl_drawable = gtk_widget_get_gl_drawable (widget);

  opengl->rebuild_view_display_list = 1;
  opengl->view_display_list = 0;

  gdk_gl_drawable_gl_begin (opengl->gl_drawable, opengl->gl_context);

  glViewport (0, 0, opengl->context_w, opengl->context_h);
  glClearColor (GUI_OPENGL_CLEAR_COLOR, GUI_OPENGL_CLEAR_COLOR, GUI_OPENGL_CLEAR_COLOR, 1.0);

  glDisable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);

  gdk_gl_drawable_gl_end (opengl->gl_drawable);

  opengl->mode = GUI_OPENGL_MODE_EDIT;
  opengl->view = GUI_OPENGL_VIEW_REGULAR;

  gui_opengl_context_redraw (opengl, NULL);
}
