/**
 *  gcode_sim.c
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

#include "gcode_sim.h"
#include <string.h>

static void
gcode_sim_intersect (gcode_t *gcode, gcode_sim_t *sim)
{
  int xind, yind, zind, min[3], max[3];
  gfloat_t pos[3], xt, yt, xd, yd, rad;

  rad = 0.5 * sim->tool_diameter + 100.0 * GCODE_PRECISION;

  /* Increment total movement */
  sim->time_elapsed += sim->step_res;

  pos[0] = sim->pos[0] + sim->origin[0];
  pos[1] = sim->pos[1] + sim->origin[1];
  pos[2] = sim->pos[2] - sim->origin[2];

  /* Take into account the origin of the cutter relative to the position of the cutter */
  min[0] = (int)((gfloat_t)gcode->voxel_number[0]) * (pos[0] - rad) / gcode->material_size[0];
  min[1] = (int)((gfloat_t)gcode->voxel_number[1]) * (pos[1] - rad) / gcode->material_size[1];
  min[2] = (int)((gfloat_t)gcode->voxel_number[2]) * (gcode->material_size[2] + pos[2]) / gcode->material_size[2];

  /* Set the cutter to be 0.5" high */
  max[0] = (int)((gfloat_t)gcode->voxel_number[0]) * (pos[0] + rad) / gcode->material_size[0];
  max[1] = (int)((gfloat_t)gcode->voxel_number[1]) * (pos[1] + rad) / gcode->material_size[1];
  /* Because tool cutting length isn't used yet, use 10x the tool radius */
  max[2] = (int)((gfloat_t)gcode->voxel_number[2]) * (gcode->material_size[2] + pos[2] + (10.0 * rad)) / gcode->material_size[2];

  if (min[0] < 0)
    min[0] = 0;

  if (min[1] < 0)
    min[1] = 0;

  if (min[2] < 0)
    min[2] = 0;

  if (max[0] < 0)
    max[0] = min[0] - 1;

  if (max[1] < 0)
    max[1] = min[1] - 1;

  if (max[2] < 0)
    max[2] = min[2] - 1;

  if (max[0] >= gcode->voxel_number[0])
    max[0] = gcode->voxel_number[0] - 1;

  if (max[1] >= gcode->voxel_number[1])
    max[1] = gcode->voxel_number[1] - 1;

  if (max[2] >= gcode->voxel_number[2])
    max[2] = gcode->voxel_number[2] - 1;

  if (min[0] >= gcode->voxel_number[0])
    min[0] = max[0] + 1;

  if (min[1] >= gcode->voxel_number[1])
    min[1] = max[1] + 1;

  if (min[2] >= gcode->voxel_number[2])
    min[2] = max[2] + 1;

  for (yind = min[1]; yind <= max[1]; yind++)
  {
    yt = ((gfloat_t)yind * sim->vn_inv[1]) * gcode->material_size[1];
    yd = yt - pos[1];

    for (xind = min[0]; xind <= max[0]; xind++)
    {
      xt = ((gfloat_t)xind * sim->vn_inv[0]) * gcode->material_size[0];
      xd = xt - pos[0];

      if ((xd * xd + yd * yd) <= rad * rad)
        for (zind = min[2]; zind <= max[2]; zind++)
          gcode->voxel_map[(zind * gcode->voxel_number[1] + yind) * gcode->voxel_number[0] + xind] = 0;
    }
  }
}

static void
gcode_sim_parse_args (gcode_sim_t *sim, char *args, gcode_vec3d_t xyz, gcode_vec3d_t ijk, gfloat_t *rad)
{
  uint8_t sind, len;
  char string[256];

  GCODE_MATH_VEC3D_SET (ijk, 0.0, 0.0, 0.0);
  *rad = 0.0;

  if (sim->absolute == 1)
  {
    GCODE_MATH_VEC3D_COPY (xyz, sim->pos);
  }
  else
  {
    GCODE_MATH_VEC3D_SET (xyz, 0.0, 0.0, 0.0);
  }

  /**
   * Extract arguments
   * Scan for 'X', 'Y', and 'Z'
   */
  sind = 0;

  while (args[sind])
  {
    sind++;

    len = strspn (&args[sind], "-.0123456789");
    memset (string, 0, 256);
    memcpy (string, &args[sind], len);

    switch (args[sind - 1])
    {
      case 'F':
      case 'f':
        sim->feed = atof (string);
        break;

      case 'I':
      case 'i':
        ijk[0] = atof (string);
        break;

      case 'J':
      case 'j':
        ijk[1] = atof (string);
        break;

      case 'K':
      case 'k':
        ijk[2] = atof (string);
        break;

      case 'R':
      case 'r':
        *rad = atof (string);
        break;

      case 'X':
      case 'x':
        xyz[0] = atof (string);
        break;

      case 'Y':
      case 'y':
        xyz[1] = atof (string);
        break;

      case 'Z':
      case 'z':
        xyz[2] = atof (string);
        break;

      default:
        break;
    }

    sind += len;
  }
}

void
gcode_sim_init (gcode_sim_t *sim, gcode_t *gcode)
{
  sim->absolute = 1;

  sim->feed = 10;
  sim->tool_diameter = 1.0;

  if (gcode->units == GCODE_UNITS_MILLIMETER)
    sim->feed *= GCODE_INCH2MM;

  sim->time_elapsed = 0.0;

  sim->step_res = 1.0 / (gfloat_t)gcode->voxel_resolution;                      /* Default to 1/voxel_res inch stepping */

  if (gcode->units == GCODE_UNITS_MILLIMETER)
    sim->step_res *= GCODE_INCH2MM;

  /**
   * Position (0,0,0) refers to the left, back, top most corner.
   * This allows us to treat the work piece as though it's in
   * Quadrant-I of a 2d cartesian map.
   */
  GCODE_MATH_VEC3D_SET (sim->pos, 0.0, 0.0, GCODE_PRECISION);
}

void
gcode_sim_free (gcode_sim_t *sim)
{
}

void
gcode_sim_G00 (gcode_t *gcode, gcode_sim_t *sim, char *args)
{
  gcode_vec3d_t xyz, ijk, dvec, orig;
  gfloat_t cur_dist, tot_dist, mag, rad;

  /**
   * Rapid Move
   * Move from the current position to the one derived from args.
   */
  gcode_sim_parse_args (sim, args, xyz, ijk, &rad);

  GCODE_MATH_VEC3D_COPY (orig, sim->pos);
  GCODE_MATH_VEC3D_DIST (tot_dist, xyz, orig);

  /* Build delta vector */
  GCODE_MATH_VEC3D_SUB (dvec, xyz, orig);

  /* If the delta vector is zero, no work to do */
  GCODE_MATH_VEC3D_MAG (mag, dvec);

  if (mag < GCODE_PRECISION)
    return;

  GCODE_MATH_VEC3D_UNITIZE (dvec);
  GCODE_MATH_VEC3D_MUL_SCALAR (dvec, dvec, sim->step_res);
  GCODE_MATH_VEC3D_MAG (mag, dvec);

  do
  {
    GCODE_MATH_VEC3D_ADD (sim->pos, sim->pos, dvec);

    GCODE_MATH_VEC3D_DIST (cur_dist, sim->pos, orig);

    /* Clamp the position once it gets close to its destination */
    if (cur_dist > tot_dist)
    {
      gcode_vec3d_t tvec;
      gfloat_t tmag;

      GCODE_MATH_VEC3D_SUB (tvec, sim->pos, xyz);
      GCODE_MATH_VEC3D_MAG (tmag, tvec);

      GCODE_MATH_VEC3D_COPY (sim->pos, xyz);
    }

    /* Perform Intersection test */
    gcode_sim_intersect (gcode, sim);
  } while (cur_dist < tot_dist);
}

void
gcode_sim_G01 (gcode_t *gcode, gcode_sim_t *sim, char *args)
{
  gcode_sim_G00 (gcode, sim, args);
}

void
gcode_sim_G02 (gcode_t *gcode, gcode_sim_t *sim, char *args)
{
  gcode_vec3d_t xyz, ijk, tvec, orig;
  gfloat_t rad, step_angle, src_angle, dst_angle;

  /**
   * Clockwise Arc
   * Move clockwise until reaching the arc length specificed
   * by xyz.
   */
  gcode_sim_parse_args (sim, args, xyz, ijk, &rad);

  if (rad > GCODE_PRECISION)                                                    /* XYZ Radius format */
  {
    /* Not implemented yet. */
  }
  else if (fabs (ijk[0]) > GCODE_PRECISION || fabs (ijk[1]) > GCODE_PRECISION || fabs (ijk[2]) > GCODE_PRECISION)       /* IJK format */
  {
    /* Calculate the origin */
    GCODE_MATH_VEC3D_ADD (orig, sim->pos, ijk);

    /**
     * Radius is determined by the magnitude of ijk.
     */
    GCODE_MATH_VEC3D_MAG (rad, ijk);

    step_angle = sim->step_res / rad;

    GCODE_MATH_VEC3D_SUB (tvec, sim->pos, orig);
    GCODE_MATH_VEC3D_UNITIZE (tvec);
    GCODE_MATH_VEC3D_ANGLE (src_angle, tvec[0], tvec[1]);

    GCODE_MATH_VEC3D_SUB (tvec, xyz, orig);
    GCODE_MATH_VEC3D_UNITIZE (tvec);
    GCODE_MATH_VEC3D_ANGLE (dst_angle, tvec[0], tvec[1]);
    /**
     * Add 2Pi to dst_angle for 2 reasons:
     * - Solves the problem of having dst_angle equal to src_angle for a complete circle.
     * - Solves the problem of going from 270 degrees clockwise to 90 degrees.
     */
    if (src_angle - GCODE_PRECISION <= dst_angle)
      src_angle += GCODE_2PI;

    /* Go from src_angle to dst_angle by step_angle increments */
    while (src_angle - step_angle > dst_angle)
    {
      tvec[0] = rad * cos (src_angle);
      tvec[1] = rad * sin (src_angle);
      tvec[2] = 0;
      GCODE_MATH_VEC3D_ADD (sim->pos, orig, tvec);
      gcode_sim_intersect (gcode, sim);

      src_angle -= step_angle;
    }
    GCODE_MATH_VEC3D_COPY (sim->pos, xyz);
  }
}

void
gcode_sim_G03 (gcode_t *gcode, gcode_sim_t *sim, char *args)
{
  gcode_vec3d_t xyz, ijk, tvec, orig;
  gfloat_t rad, step_angle, src_angle, dst_angle;

  /**
   * Clockwise Arc
   * Move clockwise until reaching the arc length specificed
   * by xyz.
   */
  gcode_sim_parse_args (sim, args, xyz, ijk, &rad);

  if (rad > GCODE_PRECISION)                                                    /* XYZ Radius format */
  {
    /* Not implemented yet. */
  }
  else if (fabs (ijk[0]) > GCODE_PRECISION || fabs (ijk[1]) > GCODE_PRECISION || fabs (ijk[2]) > GCODE_PRECISION)       /* IJK format */
  {
    /* Calculate the origin */
    GCODE_MATH_VEC3D_ADD (orig, sim->pos, ijk);

    /**
     * Radius is determined by the magnitude of ijk.
     */
    GCODE_MATH_VEC3D_MAG (rad, ijk);

    step_angle = sim->step_res / rad;

    GCODE_MATH_VEC3D_SUB (tvec, sim->pos, orig);
    GCODE_MATH_VEC3D_UNITIZE (tvec);
    GCODE_MATH_VEC3D_ANGLE (src_angle, tvec[0], tvec[1]);

    GCODE_MATH_VEC3D_SUB (tvec, xyz, orig);
    GCODE_MATH_VEC3D_UNITIZE (tvec);
    GCODE_MATH_VEC3D_ANGLE (dst_angle, tvec[0], tvec[1]);

    /**
     * Add 2Pi to dst_angle for 2 reasons:
     * - Solves the problem of having dst_angle equal to src_angle for a complete circle.
     * - Solves the problem of going from 270 degrees clockwise to 90 degrees.
     */
    if (src_angle + GCODE_PRECISION >= dst_angle)
      dst_angle += GCODE_2PI;

    /* Go from src_angle to dst_angle by step_angle increments */
    while (src_angle + step_angle < dst_angle)
    {
      tvec[0] = rad * cos (src_angle);
      tvec[1] = rad * sin (src_angle);
      tvec[2] = 0;
      GCODE_MATH_VEC3D_ADD (sim->pos, orig, tvec);
      gcode_sim_intersect (gcode, sim);

      src_angle += step_angle;
    }
    GCODE_MATH_VEC3D_COPY (sim->pos, xyz);
  }
}

void
gcode_sim_G83 (gcode_t *gcode, gcode_sim_t *sim, char *args, gfloat_t *G83_depth, gfloat_t *G83_retract, int init)
{
  gcode_vec3d_t xyz, ijk, dvec, orig;
  gfloat_t cur_dist, tot_dist, mag, retract;

  gcode_sim_parse_args (sim, args, xyz, ijk, &retract);

  if (init)
  {
    *G83_retract = retract;
    *G83_depth = xyz[2];
  }

  sim->pos[0] = xyz[0];
  sim->pos[1] = xyz[1];
  sim->pos[2] = *G83_retract;
  xyz[2] = *G83_depth;

  GCODE_MATH_VEC3D_COPY (orig, sim->pos);
  GCODE_MATH_VEC3D_DIST (tot_dist, xyz, orig);

  /* Build delta vector */
  GCODE_MATH_VEC3D_SUB (dvec, xyz, orig);

  /* If the delta vector is zero, no work to do */
  GCODE_MATH_VEC3D_MAG (mag, dvec);

  if (mag < GCODE_PRECISION)
    return;

  GCODE_MATH_VEC3D_UNITIZE (dvec);
  GCODE_MATH_VEC3D_MUL_SCALAR (dvec, dvec, sim->step_res);
  GCODE_MATH_VEC3D_MAG (mag, dvec);

  do
  {
    GCODE_MATH_VEC3D_ADD (sim->pos, sim->pos, dvec);

    GCODE_MATH_VEC3D_DIST (cur_dist, sim->pos, orig);

    /* Clamp the position once it gets close to its destination */
    if (cur_dist > tot_dist)
    {
      gcode_vec3d_t tvec;
      gfloat_t tmag;

      GCODE_MATH_VEC3D_SUB (tvec, sim->pos, xyz);
      GCODE_MATH_VEC3D_MAG (tmag, tvec);

      GCODE_MATH_VEC3D_COPY (sim->pos, xyz);
    }

    /* Perform Intersection test */
    gcode_sim_intersect (gcode, sim);
  } while (cur_dist < tot_dist);
}
