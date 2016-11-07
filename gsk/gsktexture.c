/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Benjamin Otte
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

/**
 * SECTION:GskTexture
 * @Title: GskTexture
 * @Short_description: Pixel data uploaded to a #GskRenderer
 *
 * #GskTexture is the basic element used to refer to pixel data.
 *
 * It can only be created for an existing realized #GskRenderer and becomes
 * invalid when the renderer gets unrealized.
 *
 * You cannot get your pixel data back once you've uploaded it.
 *
 * #GskTexture is an immutable structure: That means you cannot change
 * anything about it other than increasing the reference count via
 * gsk_texture_ref().
 */

#include "config.h"

#include "gsktextureprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"

/**
 * GskTexture: (ref-func gsk_texture_ref) (unref-func gsk_texture_unref)
 *
 * The `GskTexture` structure contains only private data.
 *
 * Since: 3.90
 */

G_DEFINE_BOXED_TYPE(GskTexture, gsk_texture, gsk_texture_ref, gsk_texture_unref)

static void
gsk_texture_finalize (GskTexture *self)
{    
  GSK_RENDERER_GET_CLASS (self->renderer)->texture_destroy (self);

  g_object_unref (self->renderer);

  g_free (self);
}

/**
 * gsk_texture_ref:
 * @texture: a #GskTexture
 *
 * Acquires a reference on the given #GskTexture.
 *
 * Returns: (transfer none): the #GskTexture with an additional reference
 *
 * Since: 3.90
 */
GskTexture *
gsk_texture_ref (GskTexture *texture)
{
  g_return_val_if_fail (GSK_IS_TEXTURE (texture), NULL);

  g_atomic_int_inc (&texture->ref_count);

  return texture;
}

/**
 * gsk_texture_unref:
 * @texture: a #GskTexture
 *
 * Releases a reference on the given #GskTexture.
 *
 * If the reference was the last, the resources associated to the @texture are
 * freed.
 *
 * Since: 3.90
 */
void
gsk_texture_unref (GskTexture *texture)
{
  g_return_if_fail (GSK_IS_TEXTURE (texture));

  if (g_atomic_int_dec_and_test (&texture->ref_count))
    gsk_texture_finalize (texture);
}

GskTexture *
gsk_texture_alloc (gsize           size,
                   GskRenderer    *renderer,
                   int             width,
                   int             height)
{
  GskTexture *self;

  g_assert (size >= sizeof (GskTexture));

  self = g_malloc0 (size);

  self->ref_count = 1;

  self->renderer = g_object_ref (renderer);
  self->width = width;
  self->height = height;

  return self;
}

GskTexture *
gsk_texture_new_for_data (GskRenderer  *renderer,
                          const guchar *data,
                          int           width,
                          int           height,
                          int           stride)
{
  return GSK_RENDERER_GET_CLASS (renderer)->texture_new_for_data (renderer, data, width, height, stride);
}

GskTexture *
gsk_texture_new_for_pixbuf (GskRenderer *renderer,
                            GdkPixbuf   *pixbuf)
{
  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  return GSK_RENDERER_GET_CLASS (renderer)->texture_new_for_pixbuf (renderer, pixbuf);
}

/**
 * gsk_texture_get_renderer:
 * @texture: a #GskTexture
 *
 * Returns the renderer that @texture was created for.
 *
 * Returns: (transfer none): the renderer of the #GskTexture
 *
 * Since: 3.90
 */
GskRenderer *
gsk_texture_get_renderer (GskTexture *texture)
{
  g_return_val_if_fail (GSK_IS_TEXTURE (texture), NULL);

  return texture->renderer;
}

/**
 * gsk_texture_get_width:
 * @texture: a #GskTexture
 *
 * Returns the width of @texture.
 *
 * Returns: the width of the #GskTexture
 *
 * Since: 3.90
 */
int
gsk_texture_get_width (GskTexture *texture)
{
  g_return_val_if_fail (GSK_IS_TEXTURE (texture), 0);

  return texture->width;
}

/**
 * gsk_texture_get_height:
 * @texture: a #GskTexture
 *
 * Returns the height of the @texture.
 *
 * Returns: the height of the #GskTexture
 *
 * Since: 3.90
 */
int
gsk_texture_get_height (GskTexture *texture)
{
  g_return_val_if_fail (GSK_IS_TEXTURE (texture), 0);

  return texture->height;
}

