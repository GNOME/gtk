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
 * #GskTexture is an immutable object: That means you cannot change
 * anything about it other than increasing the reference count via
 * g_object_ref().
 */

#include "config.h"

#include "gsktextureprivate.h"

#include "gskdebugprivate.h"
#include "gskrenderer.h"

#include "gdk/gdkinternals.h"

/**
 * GskTexture:
 *
 * The `GskTexture` structure contains only private data.
 *
 * Since: 3.90
 */

enum {
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_ABSTRACT_TYPE (GskTexture, gsk_texture, G_TYPE_OBJECT)

#define GSK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Texture of type '%s' does not implement GskTexture::" # method, G_OBJECT_TYPE_NAME (obj))

static void
gsk_texture_real_download (GskTexture *self,
                           guchar     *data,
                           gsize       stride)
{
  GSK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD (self, download);
}

static cairo_surface_t *
gsk_texture_real_download_surface (GskTexture *texture)
{
  cairo_surface_t *surface;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        texture->width, texture->height);
  gsk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  cairo_surface_mark_dirty (surface);

  return surface;
}

static void
gsk_texture_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GskTexture *self = GSK_TEXTURE (gobject);

  switch (prop_id)
    {
    case PROP_WIDTH:
      self->width = g_value_get_int (value);
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gsk_texture_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GskTexture *self = GSK_TEXTURE (gobject);

  switch (prop_id)
    {
    case PROP_WIDTH:
      g_value_set_int (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gsk_texture_dispose (GObject *object)
{    
  GskTexture *self = GSK_TEXTURE (object);

  gsk_texture_clear_render_data (self);

  G_OBJECT_CLASS (gsk_texture_parent_class)->dispose (object);
}

static void
gsk_texture_class_init (GskTextureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->download = gsk_texture_real_download;
  klass->download_surface = gsk_texture_real_download_surface;

  gobject_class->set_property = gsk_texture_set_property;
  gobject_class->get_property = gsk_texture_get_property;
  gobject_class->dispose = gsk_texture_dispose;

  /**
   * GskRenderer:width:
   *
   * The width of the texture.
   *
   * Since: 3.90
   */
  properties[PROP_WIDTH] =
    g_param_spec_int ("width",
                      "Width",
                      "The width of the texture",
                      1,
                      G_MAXINT,
                      1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:height:
   *
   * The height of the texture.
   *
   * Since: 3.90
   */
  properties[PROP_HEIGHT] =
    g_param_spec_int ("height",
                      "Height",
                      "The height of the texture",
                      1,
                      G_MAXINT,
                      1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gsk_texture_init (GskTexture *self)
{
}

/* GskCairoTexture */

#define GSK_TYPE_CAIRO_TEXTURE (gsk_cairo_texture_get_type ())

G_DECLARE_FINAL_TYPE (GskCairoTexture, gsk_cairo_texture, GSK, CAIRO_TEXTURE, GskTexture)

struct _GskCairoTexture {
  GskTexture parent_instance;
  cairo_surface_t *surface;
};

struct _GskCairoTextureClass {
  GskTextureClass parent_class;
};

G_DEFINE_TYPE (GskCairoTexture, gsk_cairo_texture, GSK_TYPE_TEXTURE)

static void
gsk_cairo_texture_finalize (GObject *object)
{
  GskCairoTexture *self = GSK_CAIRO_TEXTURE (object);

  cairo_surface_destroy (self->surface);

  G_OBJECT_CLASS (gsk_cairo_texture_parent_class)->finalize (object);
}

static cairo_surface_t *
gsk_cairo_texture_download_surface (GskTexture *texture)
{
  GskCairoTexture *self = GSK_CAIRO_TEXTURE (texture);

  return cairo_surface_reference (self->surface);
}

static void
gsk_cairo_texture_download (GskTexture *texture,
                            guchar     *data,
                            gsize       stride)
{
  GskCairoTexture *self = GSK_CAIRO_TEXTURE (texture);
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 texture->width, texture->height,
                                                 stride);
  cr = cairo_create (surface);

  cairo_set_source_surface (cr, self->surface, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static void
gsk_cairo_texture_class_init (GskCairoTextureClass *klass)
{
  GskTextureClass *texture_class = GSK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gsk_cairo_texture_download;
  texture_class->download_surface = gsk_cairo_texture_download_surface;

  gobject_class->finalize = gsk_cairo_texture_finalize;
}

static void
gsk_cairo_texture_init (GskCairoTexture *self)
{
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
  cairo_surface_finish (original);
  cairo_surface_destroy (original);

  return texture;
}

GskTexture *
gsk_texture_new_for_surface (cairo_surface_t *surface)
{
  GskCairoTexture *texture;

  g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);

  texture = g_object_new (GSK_TYPE_CAIRO_TEXTURE,
                          "width", cairo_image_surface_get_width (surface),
                          "height", cairo_image_surface_get_height (surface),
                          NULL);

  texture->surface = cairo_surface_reference (surface);

  return (GskTexture *) texture;
}

/* GskPixbufTexture */

#define GSK_TYPE_PIXBUF_TEXTURE (gsk_pixbuf_texture_get_type ())

G_DECLARE_FINAL_TYPE (GskPixbufTexture, gsk_pixbuf_texture, GSK, PIXBUF_TEXTURE, GskTexture)

struct _GskPixbufTexture {
  GskTexture parent_instance;

  GdkPixbuf *pixbuf;
};

struct _GskPixbufTextureClass {
  GskTextureClass parent_class;
};

G_DEFINE_TYPE (GskPixbufTexture, gsk_pixbuf_texture, GSK_TYPE_TEXTURE)

static void
gsk_pixbuf_texture_finalize (GObject *object)
{
  GskPixbufTexture *self = GSK_PIXBUF_TEXTURE (object);

  g_object_unref (self->pixbuf);

  G_OBJECT_CLASS (gsk_pixbuf_texture_parent_class)->finalize (object);
}

static void
gsk_pixbuf_texture_download (GskTexture *texture,
                             guchar     *data,
                             gsize       stride)
{
  GskPixbufTexture *self = GSK_PIXBUF_TEXTURE (texture);
  cairo_surface_t *surface;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 texture->width, texture->height,
                                                 stride);
  gdk_cairo_surface_paint_pixbuf (surface, self->pixbuf);
  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static cairo_surface_t *
gsk_pixbuf_texture_download_surface (GskTexture *texture)
{
  GskPixbufTexture *self = GSK_PIXBUF_TEXTURE (texture);

  return gdk_cairo_surface_create_from_pixbuf (self->pixbuf, 1, NULL);
}

static void
gsk_pixbuf_texture_class_init (GskPixbufTextureClass *klass)
{
  GskTextureClass *texture_class = GSK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gsk_pixbuf_texture_download;
  texture_class->download_surface = gsk_pixbuf_texture_download_surface;

  gobject_class->finalize = gsk_pixbuf_texture_finalize;
}

static void
gsk_pixbuf_texture_init (GskPixbufTexture *self)
{
}

GskTexture *
gsk_texture_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GskPixbufTexture *self;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  self = g_object_new (GSK_TYPE_PIXBUF_TEXTURE,
                       "width", gdk_pixbuf_get_width (pixbuf),
                       "height", gdk_pixbuf_get_height (pixbuf),
                       NULL);

  self->pixbuf = g_object_ref (pixbuf);

  return GSK_TEXTURE (self);
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
gsk_texture_download_surface (GskTexture *texture)
{
  return GSK_TEXTURE_GET_CLASS (texture)->download_surface (texture);
}

/**
 * gsk_texture_download:
 * @texture: a #GskTexture
 * @data: pointer to enough memory to be filled with the
 *     downloaded data of @texture
 * @stride: rowstride in bytes
 *
 * Downloads the @texture into local memory. This may be
 * an expensive operation, as the actual texture data may
 * reside on a GPU or on a remote display server.
 *
 * The data format of the downloaded data is equivalent to
 * %CAIRO_FORMAT_ARGB32, so every downloaded pixel requires
 * 4 bytes of memory.
 *
 * Downloading a texture into a Cairo image surface:
 * |[<!-- language="C" -->
 * surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
 *                                       gsk_texture_get_width (texture),
 *                                       gsk_texture_get_height (texture));
 * gsk_texture_download (texture,
 *                       cairo_image_surface_get_data (surface),
 *                       cairo_image_surface_get_stride (surface));
 * cairo_surface_mark_dirty (surface);
 * ]|
 **/
void
gsk_texture_download (GskTexture *texture,
                      guchar     *data,
                      gsize       stride)
{
  g_return_if_fail (GSK_IS_TEXTURE (texture));
  g_return_if_fail (data != NULL);
  g_return_if_fail (stride >= gsk_texture_get_width (texture) * 4);

  return GSK_TEXTURE_GET_CLASS (texture)->download (texture, data, stride);
}

gboolean
gsk_texture_set_render_data (GskTexture     *self,
                             gpointer        key,
                             gpointer        data,
                             GDestroyNotify  notify)
{
  g_return_val_if_fail (data != NULL, FALSE);
 
  if (self->render_key != NULL)
    return FALSE;

  self->render_key = key;
  self->render_data = data;
  self->render_notify = notify;

  return TRUE;
}

void
gsk_texture_clear_render_data (GskTexture *self)
{
  if (self->render_notify)
    self->render_notify (self->render_data);

  self->render_key = NULL;
  self->render_data = NULL;
  self->render_notify = NULL;
}

gpointer
gsk_texture_get_render_data (GskTexture  *self,
                             gpointer     key)
{
  if (self->render_key != key)
    return NULL;

  return self->render_data;
}
