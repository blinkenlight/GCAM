/**
 *  gcode_math.c
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

#include "gcode_math.h"
#include <stdio.h>

int
gcode_math_angle_within_arc (gfloat_t start_angle, gfloat_t sweep_angle, gfloat_t test_angle)
{
  gfloat_t begin, end;

  if (sweep_angle < 0.0)
  {
    begin = start_angle + sweep_angle;
    end = start_angle;
  }
  else
  {
    begin = start_angle;
    end = start_angle + sweep_angle;
  }

  if (begin < 0.0)
  {
    begin += 360.0;
    end += 360.0;
  }

  if ((test_angle >= begin - GCODE_ANGULAR_PRECISION) &&
      (test_angle <= end + GCODE_ANGULAR_PRECISION))
    return (0);

  if ((test_angle - 360.0 >= begin - GCODE_ANGULAR_PRECISION) &&
      (test_angle - 360.0 <= end + GCODE_ANGULAR_PRECISION))
    return (0);

  if ((test_angle + 360.0 >= begin - GCODE_ANGULAR_PRECISION) &&
      (test_angle + 360.0 <= end + GCODE_ANGULAR_PRECISION))
    return (0);

  return (1);
}

void
gcode_math_xy_to_angle (gcode_vec2d_t center, gcode_vec2d_t point, gfloat_t *angle)
{
  gfloat_t dx, dy;

  dx = point[0] - center[0];
  dy = point[1] - center[1];

  if (sqrt (dx * dx + dy * dy) < GCODE_PRECISION)                               // If the radius is practically zero, bail out by returning zero;
    *angle = 0.0;
  else                                                                          // If the radius looks valid,
    *angle = fmod (GCODE_RAD2DEG * atan2 (dy, dx) + 360.0, 360.0);              // get the arc tangent of the slope, and wrap it around to a positive value;

  if (*angle >= 360.0)                                                          // Oh, you think fmod() can never return the modulus itself? Think again...
    *angle = 0.0;
}
