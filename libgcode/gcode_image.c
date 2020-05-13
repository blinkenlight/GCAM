/**
 *  gcode_image.c
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

#include "gui_define.h"
#include "gcode_image.h"
#include "gcode_point.h"
#include "gcode_tool.h"
#include "gcode.h"
#include <libgen.h>
#include <png.h>

void
gcode_image_init (gcode_block_t **block, gcode_t *gcode, gcode_block_t *parent)
{
  gcode_image_t *image;

  *block = malloc (sizeof (gcode_block_t));

  gcode_internal_init (*block, gcode, parent, GCODE_TYPE_IMAGE, 0);

  (*block)->free = gcode_image_free;
  (*block)->save = gcode_image_save;
  (*block)->load = gcode_image_load;
  (*block)->make = gcode_image_make;
  (*block)->draw = gcode_image_draw;
  (*block)->scale = gcode_image_scale;
  (*block)->parse = gcode_image_parse;
  (*block)->clone = gcode_image_clone;

  (*block)->pdata = malloc (sizeof (gcode_image_t));

  (*block)->offref = &gcode->zero_offset;
  (*block)->offset = &gcode->zero_offset;

  strcpy ((*block)->comment, "Image");
  strcpy ((*block)->status, "OK");
  GCODE_INIT ((*block));
  GCODE_CLEAR ((*block));

  /* Defaults */

  image = (gcode_image_t *)(*block)->pdata;

  image->dmap = NULL;
  image->resolution[0] = 0;
  image->resolution[1] = 0;
  image->size[0] = GCODE_UNITS ((*block)->gcode, 1.0);
  image->size[1] = GCODE_UNITS ((*block)->gcode, 1.0);
  image->size[2] = -gcode->material_size[2];
}

void
gcode_image_free (gcode_block_t **block)
{
  gcode_image_t *image;

  image = (gcode_image_t *)(*block)->pdata;

  free (image->dmap);

  free ((*block)->code);
  free ((*block)->pdata);
  free (*block);
  *block = NULL;
}

void
gcode_image_save (gcode_block_t *block, FILE *fh)
{
  gcode_block_t *index_block;
  gcode_image_t *image;
  uint32_t size;
  uint8_t data;

  image = (gcode_image_t *)block->pdata;

  if (block->gcode->format == GCODE_FORMAT_XML)                                 // Save to new xml format
  {
    int indent = GCODE_XML_BASE_INDENT;
    gfloat_t *pmap;
    int x, y;

    index_block = block->parent;

    while (index_block)
    {
      indent++;

      index_block = index_block->parent;
    }

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_HEAD_OF_TAG (fh, GCODE_XML_TAG_IMAGE);
    GCODE_WRITE_XML_ATTR_STRING (fh, GCODE_XML_ATTR_BLOCK_COMMENT, block->comment);
    GCODE_WRITE_XML_ATTR_AS_HEX (fh, GCODE_XML_ATTR_BLOCK_FLAGS, block->flags);
    GCODE_WRITE_XML_ATTR_2D_INT (fh, GCODE_XML_ATTR_IMAGE_RESOLUTION, image->resolution);
    GCODE_WRITE_XML_ATTR_3D_FLT (fh, GCODE_XML_ATTR_IMAGE_SIZE, image->size);
    GCODE_WRITE_XML_OP_TAG_TAIL (fh);
    GCODE_WRITE_XML_END_OF_LINE (fh);

    indent++;

    pmap = image->dmap;

    for (y = 0; y < image->resolution[1]; y++)
    {
      GCODE_WRITE_XML_INDENT_TABS (fh, indent);

      for (x = 0; x < image->resolution[0]; x++)
      {
        GCODE_WRITE_XML_CONTENT_FLT (fh, *pmap);

        pmap++;
      }

      GCODE_WRITE_XML_END_OF_LINE (fh);
    }

    indent--;

    GCODE_WRITE_XML_INDENT_TABS (fh, indent);
    GCODE_WRITE_XML_END_TAG_FOR (fh, GCODE_XML_TAG_IMAGE);
    GCODE_WRITE_XML_END_OF_LINE (fh);
  }
  else                                                                          // Save to legacy binary format
  {
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_IMAGE_RESOLUTION, 2 * sizeof (int), image->resolution);
    GCODE_WRITE_BINARY_NUM_DATA (fh, GCODE_BIN_DATA_IMAGE_SIZE, 3 * sizeof (gfloat_t), image->size);
    GCODE_WRITE_BINARY_2D_ARRAY (fh, GCODE_BIN_DATA_IMAGE_DMAP, image->resolution[0], image->resolution[1], image->dmap);
  }
}

void
gcode_image_load (gcode_block_t *block, FILE *fh)
{
  gcode_image_t *image;
  uint32_t bsize, dsize, start;
  uint8_t data;

  image = (gcode_image_t *)block->pdata;

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

      case GCODE_BIN_DATA_IMAGE_RESOLUTION:
        fread (image->resolution, dsize, 1, fh);

        free (image->dmap);

        image->dmap = (gfloat_t *)calloc (image->resolution[0] * image->resolution[1], sizeof (gfloat_t));
        break;

      case GCODE_BIN_DATA_IMAGE_SIZE:
        fread (image->size, dsize, 1, fh);
        break;

      case GCODE_BIN_DATA_IMAGE_DMAP:
        fread (image->dmap, dsize, 1, fh);
        break;

      default:
        fseek (fh, dsize, SEEK_CUR);
        break;
    }
  }
}

void
gcode_image_make (gcode_block_t *block)
{
  gcode_image_t *image;
  gcode_tool_t *tool;
  gcode_vec2d_t pos;
  gfloat_t xpos, ypos;
  char string[256];
  int x, y;

  image = (gcode_image_t *)block->pdata;

  GCODE_CLEAR (block);

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  tool = gcode_tool_find (block);

  if (tool == NULL)
    return;

  GCODE_NEWLINE (block);

  sprintf (string, "IMAGE: %s", block->comment);
  GCODE_COMMENT (block, string);

  GCODE_NEWLINE (block);

  xpos = ((gfloat_t)0) * image->size[0] / (gfloat_t)image->resolution[0];
  ypos = ((gfloat_t)0) * image->size[1] / (gfloat_t)image->resolution[1];

  GCODE_MATH_VEC2D_SET (pos, xpos, ypos);
  GCODE_MATH_ROTATE (pos, pos, block->offset->rotation);
  GCODE_MATH_TRANSLATE (pos, pos, block->offset->origin);

  GCODE_RETRACT (block, block->gcode->ztraverse);

  GCODE_2D_MOVE (block, pos[0], pos[1], "");

  GCODE_PLUMMET (block, 0.0);

  for (y = 0; y < image->resolution[1]; y++)
  {
    ypos = (((gfloat_t)y) + 0.5) * image->size[1] / (gfloat_t)image->resolution[1];

    /* Even - Left to Right */
    for (x = 0; x < image->resolution[0]; x++)
    {
      xpos = (((gfloat_t)x) + 0.5) * image->size[0] / (gfloat_t)image->resolution[0];

      GCODE_MATH_VEC2D_SET (pos, xpos, ypos);
      GCODE_MATH_ROTATE (pos, pos, block->offset->rotation);
      GCODE_MATH_TRANSLATE (pos, pos, block->offset->origin);

      GCODE_3D_LINE (block, pos[0], pos[1], image->size[2] * image->dmap[y * image->resolution[0] + x], "");
    }

    y++;

    ypos = (((gfloat_t)y) + 0.5) * image->size[1] / (gfloat_t)image->resolution[1];

    if (y < image->resolution[1])
    {
      /* Odd - Left to Right */
      for (x = image->resolution[0] - 1; x >= 0; x--)
      {
        xpos = (((gfloat_t)x) + 0.5) * image->size[0] / (gfloat_t)image->resolution[0];

        GCODE_MATH_VEC2D_SET (pos, xpos, ypos);
        GCODE_MATH_ROTATE (pos, pos, block->offset->rotation);
        GCODE_MATH_TRANSLATE (pos, pos, block->offset->origin);

        GCODE_3D_LINE (block, pos[0], pos[1], image->size[2] * image->dmap[y * image->resolution[0] + x], "");
      }
    }
  }
}

void
gcode_image_draw (gcode_block_t *block, gcode_block_t *selected)
{
#if GCODE_USE_OPENGL
  gcode_image_t *image;
  gcode_vec2d_t p0, p1, p2, p3;
  int xmin, xmax, ymin, ymax, x, y, sind;
  gfloat_t xsize, ysize, zsize;
  gfloat_t xposmin, xposmax, yposmin, yposmax;
  gfloat_t xpos[2], ypos[2], zval[2][2], coef;

  if (block->flags & GCODE_FLAGS_SUPPRESS)
    return;

  image = (gcode_image_t *)block->pdata;

  xmin = ymin = 0;

  xmax = image->resolution[0];
  ymax = image->resolution[1];

  xsize = image->size[0];
  ysize = image->size[1];
  zsize = image->size[2];

  xposmin = yposmin = 0;

  xposmax = xsize;
  yposmax = ysize;

  sind = 0;

  if (selected)
    if (block == selected || block->parent == selected)
      sind = 1;

  glBegin (GL_QUADS);

  for (y = ymin; y <= ymax; y++)
  {
    ypos[0] = (y == ymin) ? yposmin : (((gfloat_t)y) - 0.5) * ysize / (gfloat_t)ymax;
    ypos[1] = (y == ymax) ? yposmax : (((gfloat_t)y) + 0.5) * ysize / (gfloat_t)ymax;

    for (x = xmin; x <= xmax; x++)
    {
      xpos[0] = (x == xmin) ? xposmin : (((gfloat_t)x) - 0.5) * xsize / (gfloat_t)xmax;
      xpos[1] = (x == xmax) ? xposmax : (((gfloat_t)x) + 0.5) * xsize / (gfloat_t)xmax;

      zval[0][0] = ((x == xmin) || (y == ymin)) ? 0 : image->dmap[(y - 1) * xmax + (x - 1)];
      zval[1][0] = ((x == xmax) || (y == ymin)) ? 0 : image->dmap[(y - 1) * xmax + (x - 0)];
      zval[0][1] = ((x == xmin) || (y == ymax)) ? 0 : image->dmap[(y - 0) * xmax + (x - 1)];
      zval[1][1] = ((x == xmax) || (y == ymax)) ? 0 : image->dmap[(y - 0) * xmax + (x - 0)];

      GCODE_MATH_VEC2D_SET (p0, xpos[0], ypos[0]);
      GCODE_MATH_VEC2D_SET (p1, xpos[1], ypos[0]);
      GCODE_MATH_VEC2D_SET (p2, xpos[1], ypos[1]);
      GCODE_MATH_VEC2D_SET (p3, xpos[0], ypos[1]);

      GCODE_MATH_ROTATE (p0, p0, block->offset->rotation);
      GCODE_MATH_ROTATE (p1, p1, block->offset->rotation);
      GCODE_MATH_ROTATE (p2, p2, block->offset->rotation);
      GCODE_MATH_ROTATE (p3, p3, block->offset->rotation);
      GCODE_MATH_TRANSLATE (p0, p0, block->offset->origin);
      GCODE_MATH_TRANSLATE (p1, p1, block->offset->origin);
      GCODE_MATH_TRANSLATE (p2, p2, block->offset->origin);
      GCODE_MATH_TRANSLATE (p3, p3, block->offset->origin);

      coef = 1.0 - 0.25 * (zval[0][0] + zval[1][0] + zval[0][1] + zval[1][1]);

      glColor3f (coef * GCODE_OPENGL_SELECTABLE_COLORS[sind][0],
                 coef * GCODE_OPENGL_SELECTABLE_COLORS[sind][1],
                 coef * GCODE_OPENGL_SELECTABLE_COLORS[sind][2]);

      glVertex3f (p0[0], p0[1], zsize * zval[0][0]);
      glVertex3f (p1[0], p1[1], zsize * zval[1][0]);
      glVertex3f (p2[0], p2[1], zsize * zval[1][1]);
      glVertex3f (p3[0], p3[1], zsize * zval[0][1]);
    }
  }

  glEnd ();
#endif
}

void
gcode_image_scale (gcode_block_t *block, gfloat_t scale)
{
  gcode_image_t *image;

  image = (gcode_image_t *)block->pdata;

  image->size[0] *= scale;
  image->size[1] *= scale;
  image->size[2] *= scale;
}

void
gcode_image_parse (gcode_block_t *block, const char **xmlattr)
{
  gcode_image_t *image;

  image = (gcode_image_t *)block->pdata;

  for (int i = 0; xmlattr[i]; i += 2)
  {
    int abc[3], m;
    unsigned int n;
    double xyz[3], w;
    const char *name, *value;

    name = xmlattr[i];
    value = xmlattr[i + 1];

    if (strcmp (name, GCODE_XML_ATTR_BLOCK_COMMENT) == 0)
    {
      GCODE_PARSE_XML_ATTR_STRING (block->comment, value);
    }
    else if (strcmp (name, GCODE_XML_ATTR_BLOCK_FLAGS) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_AS_HEX (n, value))
        block->flags = n;
    }
    else if (strcmp (name, GCODE_XML_ATTR_IMAGE_RESOLUTION) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_2D_INT (abc, value))
        for (int j = 0; j < 2; j++)
          image->resolution[j] = abc[j];
    }
    else if (strcmp (name, GCODE_XML_ATTR_IMAGE_SIZE) == 0)
    {
      if (GCODE_PARSE_XML_ATTR_3D_FLT (xyz, value))
        for (int j = 0; j < 3; j++)
          image->size[j] = (gfloat_t)xyz[j];
    }
  }

  if ((image->resolution[0] > 0) && (image->resolution[1] > 0))
  {
    free (image->dmap);

    image->dmap = (gfloat_t *)calloc (image->resolution[0] * image->resolution[1], sizeof (gfloat_t));
  }
}

void
gcode_image_clone (gcode_block_t **block, gcode_t *gcode, gcode_block_t *model)
{
  gcode_image_t *image, *model_image;
  int i;

  model_image = (gcode_image_t *)model->pdata;

  gcode_image_init (block, gcode, model->parent);

  (*block)->flags = model->flags;

  strcpy ((*block)->comment, model->comment);

  (*block)->offset = model->offset;

  image = (gcode_image_t *)(*block)->pdata;

  /* Copy resolution and size */
  image->resolution[0] = model_image->resolution[0];
  image->resolution[1] = model_image->resolution[1];
  image->size[0] = model_image->size[0];
  image->size[1] = model_image->size[1];
  image->size[2] = model_image->size[2];

  /* Copy Depth Map */
  image->dmap = malloc (image->resolution[0] * image->resolution[1] * sizeof (gfloat_t));

  for (i = 0; i < image->resolution[0] * image->resolution[1]; i++)
    image->dmap[i] = model_image->dmap[i];
}

void
gcode_image_open (gcode_block_t *block, char *filename)
{
  gcode_image_t *image;
  FILE *fp = fopen (filename, "rb");
  png_structp png_ptr;
  png_infop info_ptr;
  png_bytep *row_pointers;
  unsigned char header[8];
  int x, y, incr;

  image = (gcode_image_t *)block->pdata;

  if (!fp)
  {
    REMARK ("Failed to open file '%s'\n", basename ((char *)filename));
    return;
  }

  fread (header, 1, 8, fp);

  if (png_sig_cmp (header, 0, 8))
  {
    REMARK ("Failed to locate PNG signature in file '%s'\n", basename ((char *)filename));
    fclose (fp);
    return;
  }

  /* initialize stuff */
  png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
  {
    REMARK ("Failed to create PNG read structure for file '%s'\n", basename ((char *)filename));
    fclose (fp);
    return;
  }

  info_ptr = png_create_info_struct (png_ptr);

  if (!info_ptr)
  {
    REMARK ("Failed to create PNG info structure for file '%s'\n", basename ((char *)filename));
    png_destroy_read_struct (&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    fclose (fp);
    return;
  }

  if (setjmp (png_jmpbuf (png_ptr)))
  {
    REMARK ("Failed to read PNG info from file '%s'\n", basename ((char *)filename));
    png_destroy_read_struct (&png_ptr, &info_ptr, (png_infopp)NULL);
    fclose (fp);
    return;
  }

  png_init_io (png_ptr, fp);
  png_set_sig_bytes (png_ptr, 8);

  png_read_info (png_ptr, info_ptr);

  image->resolution[0] = png_get_image_width (png_ptr, info_ptr);
  image->resolution[1] = png_get_image_height (png_ptr, info_ptr);

  /* read file */
  if (setjmp (png_jmpbuf (png_ptr)))
  {
    REMARK ("Failed to read PNG data from file '%s'\n", basename ((char *)filename));
    png_destroy_read_struct (&png_ptr, &info_ptr, (png_infopp)NULL);
    fclose (fp);
    return;
  }

  row_pointers = malloc (sizeof (png_bytep) * image->resolution[1]);

  for (y = 0; y < image->resolution[1]; y++)
    row_pointers[y] = malloc (png_get_rowbytes (png_ptr, info_ptr));

  png_read_image (png_ptr, row_pointers);

  free (image->dmap);

  image->dmap = (gfloat_t *)calloc (image->resolution[0] * image->resolution[1], sizeof (gfloat_t));

  incr = 1;

  if (png_get_color_type (png_ptr, info_ptr) & PNG_COLOR_MASK_COLOR)
    incr = 3;

  if (png_get_color_type (png_ptr, info_ptr) & PNG_COLOR_MASK_ALPHA)
    incr = 4;

  for (y = 0; y < image->resolution[1]; y++)
  {
    png_byte *row = row_pointers[image->resolution[1] - y - 1];

    for (x = 0; x < image->resolution[0]; x++)
    {
      png_byte *ptr = &(row[x * incr]);

      if (incr == 1)
        image->dmap[y * image->resolution[0] + x] = 1.0 - 0.003921568627 * (gfloat_t)ptr[0];    /* divide by (255) */
      else
        image->dmap[y * image->resolution[0] + x] = 1.0 - 0.001307189542 * (gfloat_t)(ptr[0] + ptr[1] + ptr[2]);        /* divide by (3*255) */
    }
  }

  for (y = 0; y < image->resolution[1]; y++)
    free (row_pointers[y]);

  free (row_pointers);

  fclose (fp);
}
