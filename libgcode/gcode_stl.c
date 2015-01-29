/**
 *  gcode_stl.c
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
 *  along with this program.  If not, see <http:o//www.gnu.org/licenses/>.
 */

#include "gui_define.h"
#include "gcode_stl.h"
#include "gcode_tool.h"
#include "gcode.h"

void
gcode_stl_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_stl_t *stl;
  int i;

  *block = (gcode_block_t *)malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_STL, 0);

  (*block)->free = gcode_stl_free;
  (*block)->save = gcode_stl_save;
  (*block)->load = gcode_stl_load;
  (*block)->make = gcode_stl_make;
  (*block)->draw = gcode_stl_draw;
  (*block)->scale = gcode_stl_scale;
  (*block)->clone = gcode_stl_clone;

  (*block)->pdata = malloc (sizeof (gcode_stl_t));

  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "STL object");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  stl = (gcode_stl_t *)(*block)->pdata;

  stl->tri_num = 0;
  stl->slices = 10;
  stl->alloc_slices = stl->slices;

  stl->slice_list = (gcode_block_t **)malloc (sizeof (gcode_block_t *) * stl->alloc_slices);

  for (i = 0; i < stl->alloc_slices; i++)
    stl->slice_list[i] = NULL;

  stl->offset.side = 0.0;
  stl->offset.tool = 0.0;
  stl->offset.eval = 0.0;
  stl->offset.rotation = 0.0;
  stl->offset.origin[0] = 0.0;
  stl->offset.origin[1] = 0.0;
  stl->offset.z[0] = 0.0;
  stl->offset.z[1] = 0.0;

  (*block)->offref = &stl->offset;
}

void
gcode_stl_free (gcode_block_t **block)
{
  gcode_stl_t *stl;

  stl = (gcode_stl_t *)(*block)->pdata;
  free (stl->slice_list);
  free (stl->tri_list);

  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_stl_save (gcode_block_t *block, FILE *fh)
{
  gcode_stl_t *stl;
  uint32_t size;
  uint8_t data;

  stl = (gcode_stl_t *)block->pdata;
}

void
gcode_stl_load (gcode_block_t *block, FILE *fh)
{
  gcode_stl_t *stl;
  uint32_t bsize, dsize, start;
  uint8_t data;

  stl = (gcode_stl_t *)block->pdata;

  fread (&bsize, sizeof (uint32_t), 1, fh);

  start = ftell (fh);

  while (ftell (fh) - start < bsize)
  {
    fread (&data, sizeof (uint8_t), 1, fh);
    fread (&dsize, sizeof (uint32_t), 1, fh);

    switch (data)
    {
      case GCODE_BIN_DATA_BLOCK_COMMENT:
        fread (block->comment, sizeof (char), dsize, fh);
        break;

      case GCODE_BIN_DATA_BLOCK_FLAGS:
        fread (&block->flags, sizeof (uint8_t), dsize, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_stl_make (gcode_block_t *block)
{
  gcode_stl_t *stl;
  gcode_tool_t *tool;
  gcode_block_t *index_block;
  gfloat_t tool_radius, z;
  int i;
  char string[256];

  stl = (gcode_stl_t *)block->pdata;

  GCODE_CLEAR (block);

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  tool = gcode_tool_find (block);

  if (tool == NULL)
    return;

  tool_radius = tool->diameter * 0.5;

  /* For Each Slice */
  for (i = 0; i < stl->slices; i++)
  {
    z = block->gcode->material_size[2] * (1.0 - ((gfloat_t)i / (gfloat_t)(stl->slices - 1)));

    index_block = stl->slice_list[i];

    while (index_block)
    {
      gcode_vec2d_t e0, e1;

      index_block->ends (index_block, e0, e1, GCODE_GET);

      // glVertex3f (e0[0], e0[1], z);
      // glVertex3f (e1[0], e1[1], z);

      index_block = index_block->next;
    }
  }

  sprintf (string, "stl: %s", block->comment);
  GCODE_COMMENT (block, string);
}

void
gcode_stl_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_stl_t *stl;
  gcode_block_t *index_block;
  gfloat_t z;
  float mat_diffuse[4];
  int i;

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  stl = (gcode_stl_t *)block->pdata;

#if 0
  glEnable (GL_LIGHTING);
  glEnable (GL_LIGHT0);

  mat_diffuse[0] = 0.6;
  mat_diffuse[1] = 0.6;
  mat_diffuse[2] = 0.6;
  mat_diffuse[3] = 1.0;
  glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
  glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);

  glBegin (GL_TRIANGLES);

  for (i = 0; i < stl->tri_num; i++)
  {
    glNormal3f (stl->tri_list[i * 12 + 0], stl->tri_list[i * 12 + 1], stl->tri_list[i * 12 + 2]);
    glVertex3f (stl->tri_list[i * 12 + 3], stl->tri_list[i * 12 + 4], stl->tri_list[i * 12 + 5]);
    glVertex3f (stl->tri_list[i * 12 + 6], stl->tri_list[i * 12 + 7], stl->tri_list[i * 12 + 8]);
    glVertex3f (stl->tri_list[i * 12 + 9], stl->tri_list[i * 12 + 10], stl->tri_list[i * 12 + 11]);
  }

  glEnd ();
#else
  glDisable (GL_LIGHTING);
  glDisable (GL_LIGHT0);
  glDisable (GL_DEPTH_TEST);

  glColor3f (GCODE_OPENGL_STL_SURFACE_COLOR[0],
             GCODE_OPENGL_STL_SURFACE_COLOR[1],
             GCODE_OPENGL_STL_SURFACE_COLOR[2]);
  glBegin (GL_LINES);

  for (i = 0; i < stl->slices; i++)
  {
    z = block->gcode->material_size[2] * (1.0 - ((gfloat_t)i / (gfloat_t)(stl->slices - 1)));

    index_block = stl->slice_list[i];

    while (index_block)
    {
      gcode_vec2d_t e0, e1;

      index_block->ends (index_block, e0, e1, GCODE_GET);

      glVertex3f (e0[0], e0[1], z);
      glVertex3f (e1[0], e1[1], z);

      index_block = index_block->next;
    }
  }

  glEnd ();
#endif

#endif
}

void
gcode_stl_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_stl_t *stl;
  int i;

  stl = (gcode_stl_t *)block->pdata;

  for (i = 0; i < stl->tri_num; i++)
  {
    stl->tri_list[i * 12 + 3] *= scale;
    stl->tri_list[i * 12 + 4] *= scale;
    stl->tri_list[i * 12 + 5] *= scale;

    stl->tri_list[i * 12 + 6] *= scale;
    stl->tri_list[i * 12 + 7] *= scale;
    stl->tri_list[i * 12 + 8] *= scale;

    stl->tri_list[i * 12 + 9] *= scale;
    stl->tri_list[i * 12 + 10] *= scale;
    stl->tri_list[i * 12 + 11] *= scale;
  }
}

void
gcode_stl_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_stl_t *stl, *model_stl;

  model_stl = (gcode_stl_t *)model->pdata;
}

void
gcode_stl_build (gcode_block_t *block)
{

}

int
gcode_stl_import (gcode_block_t *block, char *filename)
{
  gcode_stl_t *stl;
  FILE *fh;
  gcode_vec3d_t nor, vec0, vec1;
  char comment[80];
  int i;

  stl = (gcode_stl_t *)block->pdata;

  fh = fopen (filename, "rb");

  if (!fh)
    return (1);

  /* First Check to see if file is Binary or ASCII */

  /* XXX - Assume Binary for the moment */
  fread (comment, 1, 80, fh);
  i = 79;
  comment[i] = 0;

  while (comment[i] == 32 || comment[i] == 0)
    comment[i--] = 0;

  fread (&stl->tri_num, sizeof (int), 1, fh);

  stl->tri_list = (float *)malloc (sizeof (float) * 12 * stl->tri_num);

  for (i = 0; i < stl->tri_num; i++)
  {
    fseek (fh, 12, SEEK_CUR);                                                   /* Skip Normal */
    fread (&stl->tri_list[12 * i + 3], sizeof (float), 9, fh);                  /* 3 vertices */
    fseek (fh, 2, SEEK_CUR);                                                    /* Skip Pad/Color */

    /* Calculate Normal */
    vec0[0] = stl->tri_list[12 * i + 6] - stl->tri_list[12 * i + 3];
    vec0[1] = stl->tri_list[12 * i + 7] - stl->tri_list[12 * i + 4];
    vec0[2] = stl->tri_list[12 * i + 8] - stl->tri_list[12 * i + 5];

    vec1[0] = stl->tri_list[12 * i + 9] - stl->tri_list[12 * i + 3];
    vec1[1] = stl->tri_list[12 * i + 10] - stl->tri_list[12 * i + 4];
    vec1[2] = stl->tri_list[12 * i + 11] - stl->tri_list[12 * i + 5];

    GCODE_MATH_VEC3D_CROSS (nor, vec0, vec1);
    GCODE_MATH_VEC3D_UNITIZE (nor);
    stl->tri_list[12 * i + 0] = nor[0];
    stl->tri_list[12 * i + 1] = nor[1];
    stl->tri_list[12 * i + 2] = nor[2];
  }

  gcode_stl_generate_slice_contours (block);
  fclose (fh);

  return (0);
}

void
gcode_stl_generate_slice_contours (gcode_block_t *block)
{
  gcode_stl_t *stl;
  int i, j, pt_num;
  gfloat_t d, t[3];
  gcode_vec3d_t pt[2];
  gcode_block_t *last_block2, **last_block, *line_block;

  stl = (gcode_stl_t *)block->pdata;

  for (i = 0; i < stl->alloc_slices; i++)
    gcode_list_free (&stl->slice_list[i]);

  free (stl->slice_list);

  stl->alloc_slices = stl->slices;
  stl->slice_list = (gcode_block_t **)malloc (sizeof (gcode_block_t *) * stl->alloc_slices);

  /**
   * Based off of material thickness (Z) generate a plane
   * intersection contours.  If geometry is higher than the
   * material thickness (Z) then ignore geometry above this
   * level.
   */
  for (i = 0; i < stl->slices; i++)
  {
    d = block->gcode->material_size[2] * (1.0 - ((gfloat_t)i / (gfloat_t)(stl->slices - 1)));

    stl->slice_list[i] = NULL;
    last_block = &stl->slice_list[i];

    /* Intersect z-plane with each triangle to generate unsorted contours from triangle geometry. */
    for (j = 0; j < stl->tri_num; j++)
    {
      /**
       * Plane Equation: Ax + By + Cz + D = 0.
       * Solving Parametrically: A(P0.x + t(P1.x-P0.x)) + B(P0.y + t(P1.y-P0.y)) + C(P0.z + t(P1.z-P0.z)) + D = 0.
       * Because the slicing plane always lies in the Z plane the normal is (0,0,1) and therefore the equation becomes:
       * P0.z + t(P1.z - P0.z)) + D = 0, where D is the slice.
       * Isolate t, if 0 < t < 1 then intersection, else no intersection.
       * Perform this test on each of the three lines that define each triangle.
       */

      /* Test 1 - Line P0, P1 */
      t[0] = (d - stl->tri_list[12 * j + 5]) / (stl->tri_list[12 * j + 8] - stl->tri_list[12 * j + 5]);

      /* Test 2 - Line P1, P2 */
      t[1] = (d - stl->tri_list[12 * j + 8]) / (stl->tri_list[12 * j + 11] - stl->tri_list[12 * j + 8]);

      /* Test 3 - Line P0, P2 */
      t[2] = (d - stl->tri_list[12 * j + 5]) / (stl->tri_list[12 * j + 11] - stl->tri_list[12 * j + 5]);

      /* XXX signedness may be incorrect. */

      /* Determine if intersection occured, if yes then calculate X,Y coordinates and store points in list */

      pt_num = 0;

      if (t[0] > 0.0 && t[0] < 1.0)                                             /* Line P0, P1 */
      {
        pt[pt_num][0] = stl->tri_list[12 * j + 3] + t[0] * (stl->tri_list[12 * j + 6] - stl->tri_list[12 * j + 3]);
        pt[pt_num][1] = stl->tri_list[12 * j + 4] + t[0] * (stl->tri_list[12 * j + 7] - stl->tri_list[12 * j + 4]);
        pt[pt_num][2] = stl->tri_list[12 * j + 5] + t[0] * (stl->tri_list[12 * j + 8] - stl->tri_list[12 * j + 5]);
        pt_num++;
      }

      if (t[1] > 0.0 && t[1] < 1.0)                                             /* Line P1, P2 */
      {
        pt[pt_num][0] = stl->tri_list[12 * j + 6] + t[1] * (stl->tri_list[12 * j + 9] - stl->tri_list[12 * j + 6]);
        pt[pt_num][1] = stl->tri_list[12 * j + 7] + t[1] * (stl->tri_list[12 * j + 10] - stl->tri_list[12 * j + 7]);
        pt[pt_num][2] = stl->tri_list[12 * j + 8] + t[1] * (stl->tri_list[12 * j + 11] - stl->tri_list[12 * j + 8]);
        pt_num++;
      }

      if (t[2] > 0.0 && t[2] < 1.0)                                             /* Line P0, P2 */
      {
        pt[pt_num][0] = stl->tri_list[12 * j + 3] + t[2] * (stl->tri_list[12 * j + 9] - stl->tri_list[12 * j + 3]);
        pt[pt_num][1] = stl->tri_list[12 * j + 4] + t[2] * (stl->tri_list[12 * j + 10] - stl->tri_list[12 * j + 4]);
        pt[pt_num][2] = stl->tri_list[12 * j + 5] + t[2] * (stl->tri_list[12 * j + 11] - stl->tri_list[12 * j + 5]);
        pt_num++;
      }

      if (pt_num == 2)
      {
        gcode_line_t *line;

        gcode_line_init (&line_block, block->gcode, NULL);
        line = (gcode_line_t *)line_block->pdata;

        line->p0[0] = pt[0][0];
        line->p0[1] = pt[0][1];

        line->p1[0] = pt[1][0];
        line->p1[1] = pt[1][1];

        if (stl->slice_list[i])
        {
          gcode_list_insert (&last_block2, line_block);
        }
        else
        {
          gcode_list_insert (&stl->slice_list[i], line_block);
        }

        last_block2 = line_block;
        last_block = &line_block;
      }
    }

    /* Reorder the lines such that they are contiguous */
    gcode_util_merge_list_fragments (&stl->slice_list[i]);
  }
}
