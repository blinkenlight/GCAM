/**
 *  gcode_sim.h
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

#ifndef _GCODE_SIM_H
#define _GCODE_SIM_H

#include "gcode_internal.h"

typedef struct gcode_sim_s
{
  gcode_vec3d_t pos;                                                            /* end mill position */
  gfloat_t tool_diameter;                                                       /* end mill diameter */
  gfloat_t origin[3];                                                           /* material origin */
  gfloat_t feed;                                                                /* units per minute */
  uint8_t absolute;                                                             /* absolute or relative coordinates */
  gfloat_t time_elapsed;                                                        /* time elapsed */
  gfloat_t step_res;                                                            /* step resolution */
  gcode_vec3d_t vn_inv;                                                         /* voxel number inverse */
} gcode_sim_t;

void gcode_sim_init (gcode_sim_t *sim, gcode_t *gcode);
void gcode_sim_free (gcode_sim_t *sim);

void gcode_sim_G00 (gcode_t *gcode, gcode_sim_t *sim, char *args);
void gcode_sim_G01 (gcode_t *gcode, gcode_sim_t *sim, char *args);
void gcode_sim_G02 (gcode_t *gcode, gcode_sim_t *sim, char *args);
void gcode_sim_G03 (gcode_t *gcode, gcode_sim_t *sim, char *args);
void gcode_sim_G83 (gcode_t *gcode, gcode_sim_t *sim, char *args, gfloat_t *G83_depth, gfloat_t *G83_retract, int init);

#endif
