/* gskngltexturelibrary.c
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

#include "gsknglcommandqueueprivate.h"
#include "gskngldriverprivate.h"
#include "gskngltexturelibraryprivate.h"

#define MAX_FRAME_AGE 60

G_DEFINE_ABSTRACT_TYPE (GskNglTextureLibrary, gsk_ngl_texture_library, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DRIVER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gsk_ngl_texture_library_constructed (GObject *object)
{
  G_OBJECT_CLASS (gsk_ngl_texture_library_parent_class)->constructed (object);

  g_assert (GSK_NGL_TEXTURE_LIBRARY (object)->hash_table != NULL);
}

static void
gsk_ngl_texture_library_dispose (GObject *object)
{
  GskNglTextureLibrary *self = (GskNglTextureLibrary *)object;

  g_clear_object (&self->driver);

  G_OBJECT_CLASS (gsk_ngl_texture_library_parent_class)->dispose (object);
}

static void
gsk_ngl_texture_library_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GskNglTextureLibrary *self = GSK_NGL_TEXTURE_LIBRARY (object);

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
gsk_ngl_texture_library_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GskNglTextureLibrary *self = GSK_NGL_TEXTURE_LIBRARY (object);

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
gsk_ngl_texture_library_class_init (GskNglTextureLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gsk_ngl_texture_library_constructed;
  object_class->dispose = gsk_ngl_texture_library_dispose;
  object_class->get_property = gsk_ngl_texture_library_get_property;
  object_class->set_property = gsk_ngl_texture_library_set_property;

  properties [PROP_DRIVER] =
    g_param_spec_object ("driver",
                         "Driver",
                         "Driver",
                         GSK_TYPE_NGL_DRIVER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gsk_ngl_texture_library_init (GskNglTextureLibrary *self)
{
}

void
gsk_ngl_texture_library_set_funcs (GskNglTextureLibrary *self,
                                   GHashFunc             hash_func,
                                   GEqualFunc            equal_func,
                                   GDestroyNotify        key_destroy,
                                   GDestroyNotify        value_destroy)
{
  g_return_if_fail (GSK_IS_NGL_TEXTURE_LIBRARY (self));
  g_return_if_fail (self->hash_table == NULL);

  self->hash_table = g_hash_table_new_full (hash_func, equal_func,
                                            key_destroy, value_destroy);
}

void
gsk_ngl_texture_library_begin_frame (GskNglTextureLibrary *self,
                                     gint64                frame_id,
                                     GPtrArray            *removed_atlases)
{
  GHashTableIter iter;

  g_return_if_fail (GSK_IS_NGL_TEXTURE_LIBRARY (self));

  if (GSK_NGL_TEXTURE_LIBRARY_GET_CLASS (self)->begin_frame)
    GSK_NGL_TEXTURE_LIBRARY_GET_CLASS (self)->begin_frame (self, frame_id, removed_atlases);

  if (removed_atlases != NULL)
    {
      GskNglTextureAtlasEntry *entry;
      guint dropped = 0;

      g_hash_table_iter_init (&iter, self->hash_table);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&entry))
        {
          if (entry->is_atlased)
            {
              for (guint i = 0; i < removed_atlases->len; i++)
                {
                  GskNglTextureAtlas *atlas = g_ptr_array_index (removed_atlases, i);

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
                  g_message ("%s: Dropped %d icons",
                             G_OBJECT_TYPE_NAME (self), dropped));
    }

  if (frame_id % MAX_FRAME_AGE == 0)
    {
      GskNglTextureAtlasEntry *entry;

      g_hash_table_iter_init (&iter, self->hash_table);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&entry))
        {
          gsk_ngl_texture_atlas_entry_mark_unused (entry);
          entry->accessed = FALSE;
        }

      GSK_NOTE (GLYPH_CACHE, g_message ("%s: %d atlas items cached",
                                        G_OBJECT_TYPE_NAME (self),
                                        g_hash_table_size (self->hash_table)));
    }
}

static GskNglTexture *
gsk_ngl_texture_library_pack_one (GskNglTextureLibrary *self,
                                  guint                 width,
                                  guint                 height)
{
  GskNglTexture *texture;

  g_assert (GSK_IS_NGL_TEXTURE_LIBRARY (self));

  if (width > self->driver->command_queue->max_texture_size ||
      height > self->driver->command_queue->max_texture_size)
    {
      g_warning ("Clipping requested texture of size %ux%u to maximum allowable size %u.",
                 width, height, self->driver->command_queue->max_texture_size);
      width = MIN (width, self->driver->command_queue->max_texture_size);
      height = MIN (height, self->driver->command_queue->max_texture_size);
    }

  texture = gsk_ngl_driver_create_texture (self->driver, width, height, GL_LINEAR, GL_LINEAR);
  texture->permanent = TRUE;

  return texture;
}

static inline gboolean
gsk_ngl_texture_atlas_pack (GskNglTextureAtlas *self,
                            int                 width,
                            int                 height,
                            int                *out_x,
                            int                *out_y)
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
gsk_ngl_texture_atlas_initialize (GskNglDriver       *driver,
                                  GskNglTextureAtlas *atlas)
{
  /* Insert a single pixel at 0,0 for use in coloring */

  gboolean packed G_GNUC_UNUSED;
  int x, y;
  guint gl_format;
  guint gl_type;
  guint8 pixel_data[4 * 3 * 3];

  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Initializing Atlas");

  packed = gsk_ngl_texture_atlas_pack (atlas, 3, 3, &x, &y);
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
gsk_ngl_texture_atlases_pack (GskNglDriver        *driver,
                              int                  width,
                              int                  height,
                              GskNglTextureAtlas **out_atlas,
                              int                 *out_x,
                              int                 *out_y)
{
  GskNglTextureAtlas *atlas = NULL;
  int x, y;

  for (guint i = 0; i < driver->atlases->len; i++)
    {
      atlas = g_ptr_array_index (driver->atlases, i);

      if (gsk_ngl_texture_atlas_pack (atlas, width, height, &x, &y))
        break;

      atlas = NULL;
    }

  if (atlas == NULL)
    {
      /* No atlas has enough space, so create a new one... */
      atlas = gsk_ngl_driver_create_atlas (driver);

      gsk_ngl_texture_atlas_initialize (driver, atlas);

      /* Pack it onto that one, which surely has enough space... */
      if (!gsk_ngl_texture_atlas_pack (atlas, width, height, &x, &y))
        g_assert_not_reached ();
    }

  *out_atlas = atlas;
  *out_x = x;
  *out_y = y;
}

gpointer
gsk_ngl_texture_library_pack (GskNglTextureLibrary *self,
                              gpointer              key,
                              gsize                 valuelen,
                              guint                 width,
                              guint                 height,
                              int                   padding,
                              guint                *out_packed_x,
                              guint                *out_packed_y)
{
  GskNglTextureAtlasEntry *entry;
  GskNglTextureAtlas *atlas = NULL;

  g_assert (GSK_IS_NGL_TEXTURE_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (valuelen > sizeof (GskNglTextureAtlasEntry));
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
  else if (width <= self->max_entry_size && height <= self->max_entry_size)
    {
      int packed_x;
      int packed_y;

      gsk_ngl_texture_atlases_pack (self->driver,
                                    padding + width + padding,
                                    padding + height + padding,
                                    &atlas,
                                    &packed_x,
                                    &packed_y);

      entry->atlas = atlas;
      entry->is_atlased = TRUE;
      entry->area.x = (float)(packed_x + padding) / atlas->width;
      entry->area.y = (float)(packed_y + padding) / atlas->height;
      entry->area.x2 = entry->area.x + (float)width / atlas->width;
      entry->area.y2 = entry->area.y + (float)height / atlas->height;

      *out_packed_x = packed_x;
      *out_packed_y = packed_y;
    }
  else
    {
      GskNglTexture *texture = gsk_ngl_texture_library_pack_one (self,
                                                                 padding + width + padding,
                                                                 padding + height + padding);

      entry->texture = texture;
      entry->is_atlased = FALSE;
      entry->accessed = TRUE;
      entry->area.x = 0.0f;
      entry->area.y = 0.0f;
      entry->area.x2 = 1.0f;
      entry->area.y2 = 1.0f;

      *out_packed_x = padding;
      *out_packed_y = padding;
    }

  g_hash_table_insert (self->hash_table, key, entry);

  return entry;
}
