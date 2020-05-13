/**
 *  gcode_code.h
 *  Source code file for G-Code generation, simulation, and visualization
 *  library.
 *
 *  Copyright (C) 2006 - 2010 by Justin Shumaker
 *  Copyright (C) 2014 - 2020 by Asztalos Attila Oszkár
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

#ifndef _GCODE_CODE_H
#define _GCODE_CODE_H

#include "gcode_internal.h"

void gcode_code_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent);
void gcode_code_free (gcode_block_t **block);
void gcode_code_save (gcode_block_t *block, FILE *fh);
void gcode_code_load (gcode_block_t *block, FILE *fh);
void gcode_code_make (gcode_block_t *block);

#endif
