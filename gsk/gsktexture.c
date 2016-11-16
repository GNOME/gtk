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
 * You cannot get your pixel data back once you've uploaded it.
 *
 * #GskTexture is an immutable structure: That means you cannot change
 * anything about it other than increasing the reference count via
 * gsk_texture_ref().
 */

#include "config.h"

#include "gsktextureprivate.h"

#include "gskdebugprivate.h"

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
  self->klass->finalize (self);

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

gpointer
gsk_texture_new (const GskTextureClass *klass,
                 int                    width,
                 int                    height)
{
  GskTexture *self;

  g_assert (klass->size >= sizeof (GskTexture));

  self = g_malloc0 (klass->size);

  self->klass = klass;
  self->ref_count = 1;

  self->width = width;
  self->height = height;

  return self;
}

/* GskCairoTexture */

typedef struct _GskCairoTexture GskCairoTexture;

struct _GskCairoTexture {
  GskTexture texture;
  cairo_surface_t *surface;
};

static void
gsk_texture_cairo_finalize (GskTexture *texture)
{
  GskCairoTexture *cairo = (GskCairoTexture *) texture;

  cairo_surface_destroy (cairo->surface);
}

static cairo_surface_t *
gsk_texture_cairo_download (GskTexture *texture)
{
  GskCairoTexture *cairo = (GskCairoTexture *) texture;

  return cairo_surface_reference (cairo->surface);
}

static const GskTextureClass GSK_TEXTURE_CLASS_CAIRO = {
  "cairo",
  sizeof (GskCairoTexture),
  gsk_texture_cairo_finalize,
  gsk_texture_cairo_download
};

GskTexture *
gsk_texture_new_for_surface (cairo_surface_t *surface)
{
  GskCairoTexture *texture;

  g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);

  texture = gsk_texture_new (&GSK_TEXTURE_CLASS_CAIRO,
                             cairo_image_surface_get_width (surface),
                             cairo_image_surface_get_height (surface));

  texture->surface = cairo_surface_reference (surface);

  return (GskTexture *) texture;
}

GskTexture *
gsk_texture_new_for_data (const guchar *data,
                          int           width,
                          int           height,
                          int           stride)
{
  GskTexture *texture;
  cairo_surface_t *original, *copy;
  cairo_t *cr;

  original = cairo_image_surface_create_for_data ((guchar *) data, CAIRO_FORMAT_ARGB32, width, height, stride);
  copy = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  cr = cairo_create (copy);
  cairo_set_source_surface (cr, original, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  texture = gsk_texture_new_for_surface (copy);

  cairo_surface_destroy (copy);
  cairo_surface_destroy (original);

  return texture;
}

/* GskPixbufTexture */

typedef struct _GskPixbufTexture GskPixbufTexture;

struct _GskPixbufTexture {
  GskTexture texture;
  GdkPixbuf *pixbuf;
};

static void
gsk_texture_pixbuf_finalize (GskTexture *texture)
{
  GskPixbufTexture *pixbuf = (GskPixbufTexture *) texture;

  g_object_unref (pixbuf->pixbuf);
}

static cairo_surface_t *
gsk_texture_pixbuf_download (GskTexture *texture)
{
  GskPixbufTexture *pixbuf = (GskPixbufTexture *) texture;

  return gdk_cairo_surface_create_from_pixbuf (pixbuf->pixbuf, 1, NULL);
}

static const GskTextureClass GSK_TEXTURE_CLASS_PIXBUF = {
  "pixbuf",
  sizeof (GskPixbufTexture),
  gsk_texture_pixbuf_finalize,
  gsk_texture_pixbuf_download
};

GskTexture *
gsk_texture_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GskPixbufTexture *texture;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  texture = gsk_texture_new (&GSK_TEXTURE_CLASS_PIXBUF,
                             gdk_pixbuf_get_width (pixbuf),
                             gdk_pixbuf_get_height (pixbuf));

  texture->pixbuf = g_object_ref (pixbuf);

  return &texture->texture;
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

cairo_surface_t *
gsk_texture_download (GskTexture *texture)
{
  return texture->klass->download (texture);
}

