/* gsknglglyphlibrary.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkmemorytextureprivate.h>
#include <gdk/gdkprofilerprivate.h>

#include "gsknglcommandqueueprivate.h"
#include "gskngldriverprivate.h"
#include "gsknglglyphlibraryprivate.h"

#define MAX_GLYPH_SIZE 128

G_DEFINE_TYPE (GskNglGlyphLibrary, gsk_ngl_glyph_library, GSK_TYPE_GL_TEXTURE_LIBRARY)

GskNglGlyphLibrary *
gsk_ngl_glyph_library_new (GskNglDriver *driver)
{
  g_return_val_if_fail (GSK_IS_NGL_DRIVER (driver), NULL);

  return g_object_new (GSK_TYPE_GL_GLYPH_LIBRARY,
                       "driver", driver,
                       NULL);
}

static guint
gsk_ngl_glyph_key_hash (gconstpointer data)
{
  const GskNglGlyphKey *key = data;

  /* We do not store the hash within the key because GHashTable will already
   * store the hash value for us and so this is called only a single time per
   * cached item. This saves an extra 4 bytes per GskNglGlyphKey which means on
   * 64-bit, we fit nicely within 2 pointers (the smallest allocation size
   * for GSlice).
   */

  return GPOINTER_TO_UINT (key->font) ^
         key->glyph ^
         (key->xshift << 24) ^
         (key->yshift << 26) ^
         key->scale;
}

static gboolean
gsk_ngl_glyph_key_equal (gconstpointer v1,
                         gconstpointer v2)
{
  return memcmp (v1, v2, sizeof (GskNglGlyphKey)) == 0;
}

static void
gsk_ngl_glyph_key_free (gpointer data)
{
  GskNglGlyphKey *key = data;

  g_clear_object (&key->font);
  g_slice_free (GskNglGlyphKey, key);
}

static void
gsk_ngl_glyph_value_free (gpointer data)
{
  g_slice_free (GskNglGlyphValue, data);
}

static void
gsk_ngl_glyph_library_begin_frame (GskNglTextureLibrary *library,
                                   gint64                frame_id,
                                   GPtrArray            *removed_atlases)
{
  GskNglGlyphLibrary *self = GSK_NGL_GLYPH_LIBRARY (library);

  memset (self->front, 0, sizeof self->front);
}

static void
gsk_ngl_glyph_library_finalize (GObject *object)
{
  GskNglGlyphLibrary *self = (GskNglGlyphLibrary *)object;

  g_clear_pointer (&self->surface_data, g_free);

  G_OBJECT_CLASS (gsk_ngl_glyph_library_parent_class)->finalize (object);
}

static void
gsk_ngl_glyph_library_class_init (GskNglGlyphLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GskNglTextureLibraryClass *library_class = GSK_NGL_TEXTURE_LIBRARY_CLASS (klass);

  object_class->finalize = gsk_ngl_glyph_library_finalize;

  library_class->begin_frame = gsk_ngl_glyph_library_begin_frame;
}

static void
gsk_ngl_glyph_library_init (GskNglGlyphLibrary *self)
{
  GSK_NGL_TEXTURE_LIBRARY (self)->max_entry_size = MAX_GLYPH_SIZE;
  gsk_ngl_texture_library_set_funcs (GSK_NGL_TEXTURE_LIBRARY (self),
                                     gsk_ngl_glyph_key_hash,
                                     gsk_ngl_glyph_key_equal,
                                     gsk_ngl_glyph_key_free,
                                     gsk_ngl_glyph_value_free);
}

static cairo_surface_t *
gsk_ngl_glyph_library_create_surface (GskNglGlyphLibrary *self,
                                      int                 stride,
                                      int                 width,
                                      int                 height,
                                      double              device_scale)
{
  cairo_surface_t *surface;
  gsize n_bytes;

  g_assert (GSK_IS_NGL_GLYPH_LIBRARY (self));
  g_assert (width > 0);
  g_assert (height > 0);

  n_bytes = stride * height;

  if G_LIKELY (n_bytes > self->surface_data_len)
    {
      self->surface_data = g_realloc (self->surface_data, n_bytes);
      self->surface_data_len = n_bytes;
    }

  memset (self->surface_data, 0, n_bytes);
  surface = cairo_image_surface_create_for_data (self->surface_data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height, stride);
  cairo_surface_set_device_scale (surface, device_scale, device_scale);

  return surface;
}

static void
render_glyph (cairo_surface_t           *surface,
              const cairo_scaled_font_t *scaled_font,
              const GskNglGlyphKey      *key,
              const GskNglGlyphValue    *value)
{
  cairo_t *cr;
  PangoGlyphString glyph_string;
  PangoGlyphInfo glyph_info;

  g_assert (surface != NULL);
  g_assert (scaled_font != NULL);

  cr = cairo_create (surface);
  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  glyph_info.glyph = key->glyph;
  glyph_info.geometry.width = value->ink_rect.width * 1024;
  if (glyph_info.glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    glyph_info.geometry.x_offset = 256 * key->xshift;
  else
    glyph_info.geometry.x_offset = 256 * key->xshift - value->ink_rect.x * 1024;
  glyph_info.geometry.y_offset = 256 * key->yshift - value->ink_rect.y * 1024;

  glyph_string.num_glyphs = 1;
  glyph_string.glyphs = &glyph_info;

  pango_cairo_show_glyph_string (cr, key->font, &glyph_string);
  cairo_destroy (cr);

  cairo_surface_flush (surface);
}

static void
gsk_ngl_glyph_library_upload_glyph (GskNglGlyphLibrary     *self,
                                    const GskNglGlyphKey   *key,
                                    const GskNglGlyphValue *value,
                                    int                     width,
                                    int                     height,
                                    double                  device_scale)
{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  cairo_scaled_font_t *scaled_font;
  GskNglTextureAtlas *atlas;
  cairo_surface_t *surface;
  guchar *pixel_data;
  guchar *free_data = NULL;
  guint gl_format;
  guint gl_type;
  guint texture_id;
  gsize stride;
  int x, y;

  g_assert (GSK_IS_NGL_GLYPH_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (value != NULL);

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)key->font);
  if G_UNLIKELY (scaled_font == NULL ||
                 cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS)
    return;

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
  atlas = value->entry.is_atlased ? value->entry.atlas : NULL;

  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Uploading glyph %d",
                                          key->glyph);

  surface = gsk_ngl_glyph_library_create_surface (self, stride, width, height, device_scale);
  render_glyph (surface, scaled_font, key, value);

  texture_id = GSK_NGL_TEXTURE_ATLAS_ENTRY_TEXTURE (value);

  g_assert (texture_id > 0);

  glPixelStorei (GL_UNPACK_ROW_LENGTH, stride / 4);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  if G_UNLIKELY (gdk_gl_context_get_use_es (gdk_gl_context_get_current ()))
    {
      pixel_data = free_data = g_malloc (width * height * 4);
      gdk_memory_convert (pixel_data,
                          width * 4,
                          GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                          cairo_image_surface_get_data (surface),
                          width * 4,
                          GDK_MEMORY_DEFAULT,
                          width, height);
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
    }
  else
    {
      pixel_data = cairo_image_surface_get_data (surface);
      gl_format = GL_BGRA;
      gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }

  if G_LIKELY (atlas != NULL)
    {
      x = atlas->width * value->entry.area.x;
      y = atlas->width * value->entry.area.y;
    }
  else
    {
      x = 0;
      y = 0;
    }

  glTexSubImage2D (GL_TEXTURE_2D, 0, x, y, width, height,
                   gl_format, gl_type, pixel_data);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

  cairo_surface_destroy (surface);
  g_free (free_data);

  gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

  GSK_NGL_TEXTURE_LIBRARY (self)->driver->command_queue->n_uploads++;

  if (gdk_profiler_is_running ())
    {
      char message[64];
      g_snprintf (message, sizeof message, "Size %dx%d", width, height);
      gdk_profiler_add_mark (start_time, GDK_PROFILER_CURRENT_TIME-start_time, "Upload Glyph", message);
    }
}

gboolean
gsk_ngl_glyph_library_add (GskNglGlyphLibrary      *self,
                           GskNglGlyphKey          *key,
                           const GskNglGlyphValue **out_value)
{
  PangoRectangle ink_rect;
  GskNglGlyphValue *value;
  int width;
  int height;
  guint packed_x;
  guint packed_y;

  g_assert (GSK_IS_NGL_GLYPH_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (out_value != NULL);

  pango_font_get_glyph_extents (key->font, key->glyph, &ink_rect, NULL);
  pango_extents_to_pixels (&ink_rect, NULL);

  if (key->xshift != 0)
    ink_rect.width++;
  if (key->yshift != 0)
    ink_rect.height++;

  width = ink_rect.width * key->scale / 1024;
  height = ink_rect.height * key->scale / 1024;

  value = gsk_ngl_texture_library_pack (GSK_NGL_TEXTURE_LIBRARY (self),
                                        key,
                                        sizeof *value,
                                        width,
                                        height,
                                        0,
                                        &packed_x, &packed_y);

  memcpy (&value->ink_rect, &ink_rect, sizeof ink_rect);

  if (key->scale > 0 && width > 0 && height > 0)
    gsk_ngl_glyph_library_upload_glyph (self,
                                        key,
                                        value,
                                        width,
                                        height,
                                        key->scale / 1024.0);

  *out_value = value;

  return GSK_NGL_TEXTURE_ATLAS_ENTRY_TEXTURE (value) != 0;
}
