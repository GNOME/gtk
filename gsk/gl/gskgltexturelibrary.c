/* gskgltexturelibrary.c
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
#include <gsk/gskdebugprivate.h>

#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"
#include "gskgltexturelibraryprivate.h"

#define DEFAULT_MAX_FRAME_AGE 60

G_DEFINE_ABSTRACT_TYPE (GskGLTextureLibrary, gsk_gl_texture_library, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DRIVER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gsk_gl_texture_library_constructed (GObject *object)
{
  G_OBJECT_CLASS (gsk_gl_texture_library_parent_class)->constructed (object);

  g_assert (GSK_GL_TEXTURE_LIBRARY (object)->hash_table != NULL);
}

static void
gsk_gl_texture_library_dispose (GObject *object)
{
  GskGLTextureLibrary *self = (GskGLTextureLibrary *)object;

  g_clear_object (&self->driver);

  G_OBJECT_CLASS (gsk_gl_texture_library_parent_class)->dispose (object);
}

static void
gsk_gl_texture_library_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GskGLTextureLibrary *self = GSK_GL_TEXTURE_LIBRARY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      g_value_set_object (value, self->driver);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gsk_gl_texture_library_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GskGLTextureLibrary *self = GSK_GL_TEXTURE_LIBRARY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      self->driver = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gsk_gl_texture_library_class_init (GskGLTextureLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gsk_gl_texture_library_constructed;
  object_class->dispose = gsk_gl_texture_library_dispose;
  object_class->get_property = gsk_gl_texture_library_get_property;
  object_class->set_property = gsk_gl_texture_library_set_property;

  properties [PROP_DRIVER] =
    g_param_spec_object ("driver",
                         "Driver",
                         "Driver",
                         GSK_TYPE_GL_DRIVER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gsk_gl_texture_library_init (GskGLTextureLibrary *self)
{
  self->max_frame_age = DEFAULT_MAX_FRAME_AGE;
}

void
gsk_gl_texture_library_set_funcs (GskGLTextureLibrary *self,
                                  GHashFunc            hash_func,
                                  GEqualFunc           equal_func,
                                  GDestroyNotify       key_destroy,
                                  GDestroyNotify       value_destroy)
{
  g_return_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self));
  g_return_if_fail (self->hash_table == NULL);

  self->hash_table = g_hash_table_new_full (hash_func, equal_func,
                                            key_destroy, value_destroy);
}

void
gsk_gl_texture_library_begin_frame (GskGLTextureLibrary *self,
                                    gint64               frame_id,
                                    GPtrArray            *removed_atlases)
{
  GHashTableIter iter;
  gboolean drop_caches = FALSE;

  g_return_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self));

  if (GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->begin_frame)
    GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->begin_frame (self, frame_id, removed_atlases);

  if (removed_atlases != NULL)
    {
      GskGLTextureAtlasEntry *entry;
      guint dropped = 0;

      g_hash_table_iter_init (&iter, self->hash_table);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&entry))
        {
          if (entry->is_atlased)
            {
              for (guint i = 0; i < removed_atlases->len; i++)
                {
                  GskGLTextureAtlas *atlas = g_ptr_array_index (removed_atlases, i);

                  if (atlas == entry->atlas)
                    {
                      g_hash_table_iter_remove (&iter);
                      dropped++;
                      break;
                    }
                }
            }
        }

      GSK_NOTE (GLYPH_CACHE,
                if (dropped > 0)
                  g_message ("%s: Dropped %d items",
                             G_OBJECT_TYPE_NAME (self), dropped));

      drop_caches |= dropped > 0;
    }

  if (frame_id % self->max_frame_age == 0)
    {
      GskGLTextureAtlasEntry *entry;
      int atlased = 0;
      int dropped = 0;

      g_hash_table_iter_init (&iter, self->hash_table);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&entry))
        {
          if (!entry->is_atlased && !entry->accessed)
            {
              gsk_gl_driver_release_texture (self->driver, entry->texture);
              g_hash_table_iter_remove (&iter);
              dropped++;
              continue;
            }

          gsk_gl_texture_atlas_entry_mark_unused (entry);
          entry->accessed = FALSE;
          if (entry->is_atlased)
            atlased++;
        }

      GSK_NOTE (GLYPH_CACHE, g_message ("%s: Dropped %d individual items",
                                        G_OBJECT_TYPE_NAME (self),
                                        dropped);
                             g_message ("%s: %d items cached (%d atlased, %d individually)",
                                        G_OBJECT_TYPE_NAME (self),
                                        g_hash_table_size (self->hash_table),
                                        atlased,
                                        g_hash_table_size (self->hash_table) - atlased));

      drop_caches |= dropped > 0;
    }

  if (drop_caches && GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->clear_cache)
    GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->clear_cache (self);
}

static GskGLTexture *
gsk_gl_texture_library_pack_one (GskGLTextureLibrary *self,
                                 guint                width,
                                 guint                height)
{
  GskGLTexture *texture;

  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));

  if (width > self->driver->command_queue->max_texture_size ||
      height > self->driver->command_queue->max_texture_size)
    {
      g_warning ("Clipping requested texture of size %ux%u to maximum allowable size %u.",
                 width, height, self->driver->command_queue->max_texture_size);
      width = MIN (width, self->driver->command_queue->max_texture_size);
      height = MIN (height, self->driver->command_queue->max_texture_size);
    }

  texture = gsk_gl_driver_create_texture (self->driver, width, height, GL_RGBA8, GL_LINEAR, GL_LINEAR);
  texture->permanent = TRUE;

  return texture;
}

static inline gboolean
gsk_gl_texture_atlas_pack (GskGLTextureAtlas *self,
                           int                width,
                           int                height,
                           int               *out_x,
                           int               *out_y)
{
  stbrp_rect rect;

  rect.w = width;
  rect.h = height;

  stbrp_pack_rects (&self->context, &rect, 1);

  if (rect.was_packed)
    {
      *out_x = rect.x;
      *out_y = rect.y;
    }

  return rect.was_packed;
}

static void
gsk_gl_texture_atlas_initialize (GskGLDriver       *driver,
                                 GskGLTextureAtlas *atlas)
{
  /* Insert a single pixel at 0,0 for use in coloring */

  gboolean packed G_GNUC_UNUSED;
  int x, y;
  guint gl_format;
  guint gl_type;
  guint8 pixel_data[4 * 3 * 3];

  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Initializing Atlas");

  packed = gsk_gl_texture_atlas_pack (atlas, 3, 3, &x, &y);
  g_assert (packed);
  g_assert (x == 0 && y == 0);

  memset (pixel_data, 255, sizeof pixel_data);

  if (gdk_gl_context_get_use_es (gdk_gl_context_get_current ()))
    {
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
    }
  else
    {
      gl_format = GL_BGRA;
      gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }

  glBindTexture (GL_TEXTURE_2D, atlas->texture_id);

  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   0, 0,
                   3, 3,
                   gl_format, gl_type,
                   pixel_data);

  gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

  driver->command_queue->n_uploads++;
}

static void
gsk_gl_texture_atlases_pack (GskGLDriver        *driver,
                             int                 width,
                             int                 height,
                             GskGLTextureAtlas **out_atlas,
                             int                *out_x,
                             int                *out_y)
{
  GskGLTextureAtlas *atlas = NULL;
  int x, y;

  for (guint i = 0; i < driver->atlases->len; i++)
    {
      atlas = g_ptr_array_index (driver->atlases, i);

      if (gsk_gl_texture_atlas_pack (atlas, width, height, &x, &y))
        break;

      atlas = NULL;
    }

  if (atlas == NULL)
    {
      /* No atlas has enough space, so create a new one... */
      atlas = gsk_gl_driver_create_atlas (driver);

      gsk_gl_texture_atlas_initialize (driver, atlas);

      /* Pack it onto that one, which surely has enough space... */
      if (!gsk_gl_texture_atlas_pack (atlas, width, height, &x, &y))
        g_assert_not_reached ();
    }

  *out_atlas = atlas;
  *out_x = x;
  *out_y = y;
}

gpointer
gsk_gl_texture_library_pack (GskGLTextureLibrary *self,
                             gpointer             key,
                             gsize                valuelen,
                             guint                width,
                             guint                height,
                             int                  padding,
                             guint               *out_packed_x,
                             guint               *out_packed_y)
{
  GskGLTextureAtlasEntry *entry;
  GskGLTextureAtlas *atlas = NULL;

  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (valuelen > sizeof (GskGLTextureAtlasEntry));
  g_assert (out_packed_x != NULL);
  g_assert (out_packed_y != NULL);

  entry = g_slice_alloc0 (valuelen);
  entry->n_pixels = width * height;
  entry->accessed = TRUE;
  entry->used = TRUE;

  /* If our size is invisible then we just want an entry in the
   * cache for faster lookups, but do not actually spend any texture
   * allocations on this entry.
   */
  if (width <= 0 && height <= 0)
    {
      entry->is_atlased = FALSE;
      entry->texture = NULL;
      entry->area.x = 0.0f;
      entry->area.y = 0.0f;
      entry->area.x2 = 0.0f;
      entry->area.y2 = 0.0f;

      *out_packed_x = 0;
      *out_packed_y = 0;
    }
  else if (self->max_entry_size == 0 ||
           (width <= self->max_entry_size &&
            height <= self->max_entry_size))
    {
      int packed_x;
      int packed_y;

      gsk_gl_texture_atlases_pack (self->driver,
                                   padding + width + padding,
                                   padding + height + padding,
                                   &atlas,
                                   &packed_x,
                                   &packed_y);

      entry->atlas = atlas;
      entry->is_atlased = TRUE;
      entry->area.x = (packed_x + padding) / (float)atlas->width;
      entry->area.y = (packed_y + padding) / (float)atlas->height;
      entry->area.x2 = (packed_x + padding + width) / (float)atlas->width;
      entry->area.y2 = (packed_y + padding + height) / (float)atlas->height;

      *out_packed_x = packed_x;
      *out_packed_y = packed_y;
    }
  else
    {
      GskGLTexture *texture = gsk_gl_texture_library_pack_one (self,
                                                               padding + width + padding,
                                                               padding + height + padding);

      entry->texture = texture;
      entry->is_atlased = FALSE;
      entry->accessed = TRUE;
      entry->area.x = padding / (float) (padding + width + padding);
      entry->area.y = padding / (float) (padding + height + padding);
      entry->area.x2 = (padding + width) / (float) (padding + width + padding);
      entry->area.y2 = (padding + height) / (float) (padding + height + padding);

      *out_packed_x = 0;
      *out_packed_y = 0;
    }

  g_hash_table_insert (self->hash_table, key, entry);

  return entry;
}
