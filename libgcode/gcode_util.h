/**
 *  gcode_util.h
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

#ifndef _GCODE_UTIL_H
#define _GCODE_UTIL_H

#include "gcode_internal.h"

int gcode_util_xml_safelen (char *string);
void gcode_util_xml_cpysafe (char *safestring, char *string);
int gcode_util_qsort_compare_asc (const void *a, const void *b);
void gcode_util_remove_spaces (char *string);
void gcode_util_remove_comment (char *string);
void gcode_util_remove_duplicate_scalars (gfloat_t *array, uint32_t *num);
int gcode_util_intersect (gcode_block_t *block_a, gcode_block_t *block_b, gcode_vec2d_t ip_array[2], int *ip_num);
void gcode_util_fillet (gcode_block_t *line1, gcode_block_t *line2, gcode_block_t *fillet_arc, gfloat_t radius);
void gcode_util_flip_direction (gcode_block_t *block);
int gcode_util_get_sublist_snapshot (gcode_block_t **listhead, gcode_block_t *start_block, gcode_block_t *end_block);
int gcode_util_remove_null_sections (gcode_block_t **listhead);
int gcode_util_merge_list_fragments (gcode_block_t **listhead);
int gcode_util_convert_to_no_offset (gcode_block_t *listhead);

#endif
