/* gskngltexturepool.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdktextureprivate.h>

#include "gskngltexturepoolprivate.h"
#include "ninesliceprivate.h"

void
gsk_ngl_texture_free (GskNglTexture *texture)
{
  if (texture != NULL)
    {
      g_assert (texture->link.prev == NULL);
      g_assert (texture->link.next == NULL);

      if (texture->user)
        g_clear_pointer (&texture->user, gdk_texture_clear_render_data);

      if (texture->texture_id != 0)
        {
          glDeleteTextures (1, &texture->texture_id);
          texture->texture_id = 0;
        }

      for (guint i = 0; i < texture->n_slices; i++)
        {
          glDeleteTextures (1, &texture->slices[i].texture_id);
          texture->slices[i].texture_id = 0;
        }

      g_clear_pointer (&texture->slices, g_free);
      g_clear_pointer (&texture->nine_slice, g_free);

      g_slice_free (GskNglTexture, texture);
    }
}

void
gsk_ngl_texture_pool_init (GskNglTexturePool *self)
{
  g_queue_init (&self->queue);
}

void
gsk_ngl_texture_pool_clear (GskNglTexturePool *self)
{
  guint *free_me = NULL;
  guint *texture_ids;
  guint i = 0;

  if G_LIKELY (self->queue.length <= 1024)
    texture_ids = g_newa (guint, self->queue.length);
  else
    texture_ids = free_me = g_new (guint, self->queue.length);

  while (self->queue.length > 0)
    {
      GskNglTexture *head = g_queue_peek_head (&self->queue);

      g_queue_unlink (&self->queue, &head->link);

      texture_ids[i++] = head->texture_id;
      head->texture_id = 0;

      gsk_ngl_texture_free (head);
    }

  g_assert (self->queue.length == 0);

  if (i > 0)
    glDeleteTextures (i, texture_ids);

  g_free (free_me);
}

void
gsk_ngl_texture_pool_put (GskNglTexturePool *self,
                          GskNglTexture     *texture)
{
  g_assert (self != NULL);
  g_assert (texture != NULL);
  g_assert (texture->user == NULL);
  g_assert (texture->link.prev == NULL);
  g_assert (texture->link.next == NULL);
  g_assert (texture->link.data == texture);

  if (texture->permanent)
    gsk_ngl_texture_free (texture);
  else
    g_queue_push_tail_link (&self->queue, &texture->link);
}

GskNglTexture *
gsk_ngl_texture_pool_get (GskNglTexturePool *self,
                          int                width,
                          int                height,
                          int                min_filter,
                          int                mag_filter)
{
  GskNglTexture *texture;

  g_assert (self != NULL);

  texture = g_slice_new0 (GskNglTexture);
  texture->link.data = texture;
  texture->min_filter = min_filter;
  texture->mag_filter = mag_filter;

  glGenTextures (1, &texture->texture_id);

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, texture->texture_id);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (gdk_gl_context_get_use_es (gdk_gl_context_get_current ()))
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

  glBindTexture (GL_TEXTURE_2D, 0);

  return texture;
}

GskNglTexture *
gsk_ngl_texture_new (guint  texture_id,
                     int    width,
                     int    height,
                     int    min_filter,
                     int    mag_filter,
                     gint64 frame_id)
{
  GskNglTexture *texture;

  texture = g_slice_new0 (GskNglTexture);
  texture->texture_id = texture_id;
  texture->link.data = texture;
  texture->min_filter = min_filter;
  texture->mag_filter = mag_filter;
  texture->width = width;
  texture->height = height;
  texture->last_used_in_frame = frame_id;

  return texture;
}

const GskNglTextureNineSlice *
gsk_ngl_texture_get_nine_slice (GskNglTexture        *texture,
                                const GskRoundedRect *outline,
                                float                 extra_pixels_x,
                                float                 extra_pixels_y)
{
  g_assert (texture != NULL);
  g_assert (outline != NULL);

  if G_UNLIKELY (texture->nine_slice == NULL)
    {
      texture->nine_slice = g_new0 (GskNglTextureNineSlice, 9);

      nine_slice_rounded_rect (texture->nine_slice, outline);
      nine_slice_grow (texture->nine_slice, extra_pixels_x, extra_pixels_y);
      nine_slice_to_texture_coords (texture->nine_slice, texture->width, texture->height);
    }

  return texture->nine_slice;
}
