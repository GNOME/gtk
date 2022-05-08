/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2021 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkpngprivate.h"

#include <glib/gi18n-lib.h>
#include "gdkcolorspaceprivate.h"
#include "gdklcmscolorspaceprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdkprofilerprivate.h"
#include "gdktexture.h"
#include "gdktextureprivate.h"
#include "gsk/gl/fp16private.h"

#include <lcms2.h>
#include <png.h>
#include <stdio.h>

/* The main difference between the png load/save code here and
 * gdk-pixbuf is that we can support loading 16-bit data in the
 * future, and we can extract gamma and colorspace information
 * to produce linear, color-corrected data.
 */

/* {{{ Callbacks */

/* No sigsetjmp on Windows */
#ifndef HAVE_SIGSETJMP
#define sigjmp_buf jmp_buf
#define sigsetjmp(jb, x) setjmp(jb)
#define siglongjmp longjmp
#endif

typedef struct
{
  guchar *data;
  gsize size;
  gsize position;
} png_io;


static void
png_read_func (png_structp png,
               png_bytep   data,
               png_size_t  size)
{
  png_io *io;

  io = png_get_io_ptr (png);

  if (io->position + size > io->size)
    png_error (png, "Read past EOF");

  memcpy (data, io->data + io->position, size);
  io->position += size;
}

static void
png_write_func (png_structp png,
                png_bytep   data,
                png_size_t  size)
{
  png_io *io;

  io = png_get_io_ptr (png);

  if (io->position > io->size ||
      io->size - io->position  < size)
    {
      io->size = io->position + size;
      io->data = g_realloc (io->data, io->size);
    }

  memcpy (io->data + io->position, data, size);
  io->position += size;
}

static void
png_flush_func (png_structp png)
{
}

static png_voidp
png_malloc_callback (png_structp o,
                     png_size_t  size)
{
  return g_try_malloc (size);
}

static void
png_free_callback (png_structp o,
                   png_voidp   x)
{
  g_free (x);
}

G_GNUC_NORETURN static void
png_simple_error_callback (png_structp     png,
                           png_const_charp error_msg)
{
  GError **error = png_get_error_ptr (png);

  if (error && !*error)
    g_set_error (error,
                 GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_CORRUPT_IMAGE,
                 _("Error reading png (%s)"), error_msg);

  longjmp (png_jmpbuf (png), 1);
}

static void
png_simple_warning_callback (png_structp     png,
                             png_const_charp error_msg)
{
}

/* }}} */
/* {{{ Color profile handling */

static GdkColorSpace *
gdk_png_get_color_space (png_struct *png,
                         png_info   *info)
{
  GdkColorSpace *color_space;
  guchar *icc_data;
  png_uint_32 icc_len;
  char *name;
  double gamma;
  cmsCIExyY whitepoint;
  cmsCIExyYTRIPLE primaries;
  cmsToneCurve *curve;
  cmsHPROFILE lcms_profile;
  int intent;

  if (png_get_iCCP (png, info, &name, NULL, &icc_data, &icc_len))
    {
      GBytes *bytes = g_bytes_new (icc_data, icc_len);

      color_space = gdk_color_space_new_from_icc_profile (bytes, NULL);
      g_bytes_unref (bytes);
      if (color_space)
        return color_space;
    }

  if (png_get_sRGB (png, info, &intent))
    return g_object_ref (gdk_color_space_get_srgb ());

  /* If neither of those is valid, the result is sRGB */
  if (!png_get_valid (png, info, PNG_INFO_gAMA) &&
      !png_get_valid (png, info, PNG_INFO_cHRM))
    return g_object_ref (gdk_color_space_get_srgb ());

  if (!png_get_gAMA (png, info, &gamma))
    gamma = 2.4;

  if (!png_get_cHRM (png, info,
                     &whitepoint.x, &whitepoint.y,
                     &primaries.Red.x, &primaries.Red.y,
                     &primaries.Green.x, &primaries.Green.y,
                     &primaries.Blue.x, &primaries.Blue.y))
    {
      if (gamma == 2.4)
        return g_object_ref (gdk_color_space_get_srgb ());

      whitepoint = (cmsCIExyY) { 0.3127, 0.3290, 1.0 };
      primaries = (cmsCIExyYTRIPLE) {
                    { 0.6400, 0.3300, 1.0 },
                    { 0.3000, 0.6000, 1.0 },
                    { 0.1500, 0.0600, 1.0 }
                  };
    }
  else
    {
      primaries.Red.Y = 1.0;
      primaries.Green.Y = 1.0;
      primaries.Blue.Y = 1.0;
    }

  curve = cmsBuildGamma (NULL, 1.0 / gamma);
  lcms_profile = cmsCreateRGBProfile (&whitepoint,
                                      &primaries,
                                      (cmsToneCurve*[3]) { curve, curve, curve });
  color_space = gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);
  /* FIXME: errors? */
  if (color_space == NULL)
    color_space = g_object_ref (gdk_color_space_get_srgb ());
  cmsFreeToneCurve (curve);

  return color_space;
}

static void
gdk_png_set_color_space (png_struct    *png,
                         png_info      *info,
                         GdkColorSpace *color_space)
{
  /* FIXME: allow deconstructing RGB profiles into gAMA and cHRM
   * instead of falling back to iCCP
   */
  if (color_space == gdk_color_space_get_srgb ())
    {
      png_set_sRGB_gAMA_and_cHRM (png, info, /* FIXME */ PNG_sRGB_INTENT_PERCEPTUAL);
    }
  else
    {
      GBytes *bytes = gdk_color_space_save_to_icc_profile (color_space, NULL);
      png_set_iCCP (png, info,
                    "ICC profile",
                    0,
                    g_bytes_get_data (bytes, NULL),
                    g_bytes_get_size (bytes));
      g_bytes_unref (bytes);
    }
}

 /* }}} */
 /* {{{ Public API */

GdkTexture *
gdk_load_png (GBytes  *bytes,
              GError **error)
{
  png_io io;
  png_struct *png = NULL;
  png_info *info;
  guint width, height;
  int depth, color_type;
  int interlace, stride;
  GdkMemoryFormat format;
  guchar *buffer = NULL;
  guchar **row_pointers = NULL;
  GBytes *out_bytes;
  GdkColorSpace *color_space;
  GdkTexture *texture;
  int bpp;
  G_GNUC_UNUSED gint64 before = GDK_PROFILER_CURRENT_TIME;

  io.data = (guchar *)g_bytes_get_data (bytes, &io.size);
  io.position = 0;

  png = png_create_read_struct_2 (PNG_LIBPNG_VER_STRING,
                                  error,
                                  png_simple_error_callback,
                                  png_simple_warning_callback,
                                  NULL,
                                  png_malloc_callback,
                                  png_free_callback);
  if (png == NULL)
    g_error ("Out of memory");

  info = png_create_info_struct (png);
  if (info == NULL)
    g_error ("Out of memory");

  png_set_read_fn (png, &io, png_read_func);

  if (sigsetjmp (png_jmpbuf (png), 1))
    {
      g_free (buffer);
      g_free (row_pointers);
      png_destroy_read_struct (&png, &info, NULL);
      return NULL;
    }

  png_read_info (png, info);

  png_get_IHDR (png, info,
                &width, &height, &depth,
                &color_type, &interlace, NULL, NULL);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb (png);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_expand_gray_1_2_4_to_8 (png);

  if (png_get_valid (png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha (png);

  if (depth < 8)
    png_set_packing (png);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb (png);

  if (interlace != PNG_INTERLACE_NONE)
    png_set_interlace_handling (png);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  png_set_swap (png);
#endif

  png_read_update_info (png, info);
  png_get_IHDR (png, info,
                &width, &height, &depth,
                &color_type, &interlace, NULL, NULL);
  if (depth != 8 && depth != 16)
    {
      png_destroy_read_struct (&png, &info, NULL);
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   _("Unsupported depth %u in png image"), depth);
      return NULL;
    }

  switch (color_type)
    {
    case PNG_COLOR_TYPE_RGB_ALPHA:
      if (depth == 8)
        {
          format = GDK_MEMORY_R8G8B8A8;
        }
      else
        {
          format = GDK_MEMORY_R16G16B16A16;
        }
      break;
    case PNG_COLOR_TYPE_RGB:
      if (depth == 8)
        {
          format = GDK_MEMORY_R8G8B8;
        }
      else if (depth == 16)
        {
          format = GDK_MEMORY_R16G16B16;
        }
      break;
    default:
      png_destroy_read_struct (&png, &info, NULL);
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT,
                   _("Unsupported color type %u in png image"), color_type);
      return NULL;
    }

  color_space = gdk_png_get_color_space (png, info);

  bpp = gdk_memory_format_bytes_per_pixel (format);
  stride = width * bpp;
  if (stride % 8)
    stride += 8 - stride % 8;

  buffer = g_try_malloc_n (height, stride);
  row_pointers = g_try_malloc_n (height, sizeof (char *));

  if (!buffer || !row_pointers)
    {
      g_object_unref (color_space);
      g_free (buffer);
      g_free (row_pointers);
      png_destroy_read_struct (&png, &info, NULL);
      g_set_error (error,
                   GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_TOO_LARGE,
                   _("Not enough memory for image size %ux%u"), width, height);
      return NULL;
    }

  for (int i = 0; i < height; i++)
    row_pointers[i] = &buffer[i * stride];

  png_read_image (png, row_pointers);
  png_read_end (png, info);

  out_bytes = g_bytes_new_take (buffer, height * stride);
  texture = gdk_memory_texture_new_with_color_space (width, height,
                                                     format, color_space,
                                                     out_bytes, stride);
  g_bytes_unref (out_bytes);
  g_object_unref (color_space);

  g_free (row_pointers);
  png_destroy_read_struct (&png, &info, NULL);

  if (GDK_PROFILER_IS_RUNNING)
    {
      gint64 end = GDK_PROFILER_CURRENT_TIME;
      if (end - before > 500000)
        gdk_profiler_add_mark (before, end - before, "png load", NULL);
    }

  return texture;
}

GBytes *
gdk_save_png (GdkTexture *texture)
{
  png_struct *png = NULL;
  png_info *info;
  png_io io = { NULL, 0, 0 };
  int width, height;
  gsize stride;
  const guchar *data;
  int y;
  GdkMemoryTexture *memtex;
  GdkMemoryFormat format;
  GdkColorSpace *color_space;
  int png_format;
  int depth;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  color_space = gdk_texture_get_color_space (texture);
  format = gdk_texture_get_format (texture);

  memtex = gdk_memory_texture_from_texture (texture, format, color_space);

  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      format = GDK_MEMORY_R8G8B8A8;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
      format = GDK_MEMORY_A8B8G8R8;
#endif
      png_format = PNG_COLOR_TYPE_RGB_ALPHA;
      depth = 8;
      break;

    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      format = GDK_MEMORY_R8G8B8;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
      format = GDK_MEMORY_B8G8R8;
#endif
      png_format = PNG_COLOR_TYPE_RGB;
      depth = 8;
      break;

    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      format = GDK_MEMORY_R16G16B16A16;
      png_format = PNG_COLOR_TYPE_RGB_ALPHA;
      depth = 16;
      break;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
      format = GDK_MEMORY_R16G16B16;
      png_format = PNG_COLOR_TYPE_RGB;
      depth = 16;
      break;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
    }

  png = png_create_write_struct_2 (PNG_LIBPNG_VER_STRING, NULL,
                                   png_simple_error_callback,
                                   png_simple_warning_callback,
                                   NULL,
                                   png_malloc_callback,
                                   png_free_callback);
  if (!png)
    return NULL;

  info = png_create_info_struct (png);
  if (!info)
    {
      png_destroy_read_struct (&png, NULL, NULL);
      return NULL;
    }

  memtex = gdk_memory_texture_from_texture (texture, format, gdk_color_space_get_srgb ());

  if (sigsetjmp (png_jmpbuf (png), 1))
    {
      g_object_unref (memtex);
      g_free (io.data);
      png_destroy_read_struct (&png, &info, NULL);
      return NULL;
    }

  png_set_write_fn (png, &io, png_write_func, png_flush_func);

  png_set_IHDR (png, info, width, height, depth,
                png_format,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);

  gdk_png_set_color_space (png, info, color_space);

  png_write_info (png, info);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  png_set_swap (png);
#endif

  data = gdk_memory_texture_get_data (memtex);
  stride = gdk_memory_texture_get_stride (memtex);
  for (y = 0; y < height; y++)
    png_write_row (png, data + y * stride);

  png_write_end (png, info);

  png_destroy_write_struct (&png, &info);

  g_object_unref (memtex);

  return g_bytes_new_take (io.data, io.size);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
