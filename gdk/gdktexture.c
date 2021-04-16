/* gdktexture.c
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
 * GdkTexture:
 *
 * `GdkTexture` is the basic element used to refer to pixel data.
 *
 * It is primarily meant for pixel data that will not change over
 * multiple frames, and will be used for a long time.
 *
 * There are various ways to create `GdkTexture` objects from a
 * `GdkPixbuf`, or a Cairo surface, or other pixel data.
 *
 * The ownership of the pixel data is transferred to the `GdkTexture`
 * instance; you can only make a copy of it, via
 * [method@Gdk.Texture.download].
 *
 * `GdkTexture` is an immutable object: That means you cannot change
 * anything about it other than increasing the reference count via
 * g_object_ref().
 */

#include "config.h"

#include "gdktextureprivate.h"

#include "gdkinternals.h"
#include "gdkmemorytextureprivate.h"
#include "gdkpaintable.h"
#include "gdksnapshot.h"

#include <graphene.h>

/* HACK: So we don't need to include any (not-yet-created) GSK or GTK headers */
void
gtk_snapshot_append_texture (GdkSnapshot            *snapshot,
                             GdkTexture             *texture,
                             const graphene_rect_t  *bounds);

enum {
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gdk_texture_paintable_snapshot (GdkPaintable *paintable,
                                GdkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  GdkTexture *self = GDK_TEXTURE (paintable);

  gtk_snapshot_append_texture (snapshot,
                               self,
                               &GRAPHENE_RECT_INIT (0, 0, width, height));
}

static GdkPaintableFlags
gdk_texture_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE
       | GDK_PAINTABLE_STATIC_CONTENTS;
}

static int
gdk_texture_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GdkTexture *self = GDK_TEXTURE (paintable);

  return self->width;
}

static int
gdk_texture_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GdkTexture *self = GDK_TEXTURE (paintable);

  return self->height;
}

static void
gdk_texture_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gdk_texture_paintable_snapshot;
  iface->get_flags = gdk_texture_paintable_get_flags;
  iface->get_intrinsic_width = gdk_texture_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gdk_texture_paintable_get_intrinsic_height;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdkTexture, gdk_texture, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                         gdk_texture_paintable_init))

#define GDK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Texture of type '%s' does not implement GdkTexture::" # method, G_OBJECT_TYPE_NAME (obj))

static void
gdk_texture_real_download (GdkTexture         *self,
                           const GdkRectangle *area,
                           guchar             *data,
                           gsize               stride)
{
  GDK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD (self, download);
}

static void
gdk_texture_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GdkTexture *self = GDK_TEXTURE (gobject);

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
gdk_texture_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkTexture *self = GDK_TEXTURE (gobject);

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
gdk_texture_dispose (GObject *object)
{
  GdkTexture *self = GDK_TEXTURE (object);

  gdk_texture_clear_render_data (self);

  G_OBJECT_CLASS (gdk_texture_parent_class)->dispose (object);
}

static void
gdk_texture_class_init (GdkTextureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->download = gdk_texture_real_download;

  gobject_class->set_property = gdk_texture_set_property;
  gobject_class->get_property = gdk_texture_get_property;
  gobject_class->dispose = gdk_texture_dispose;

  /**
   * GdkTexture:width: (attributes org.gtk.Property.get=gdk_texture_get_width)
   *
   * The width of the texture, in pixels.
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
   * GdkTexture:height: (attributes org.gtk.Property.get=gdk_texture_get_height)
   *
   * The height of the texture, in pixels.
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
gdk_texture_init (GdkTexture *self)
{
}

/**
 * gdk_texture_new_for_surface:
 * @surface: a cairo image surface
 *
 * Creates a new texture object representing the surface.
 *
 * @surface must be an image surface with format CAIRO_FORMAT_ARGB32.
 *
 * Returns: a new `GdkTexture`
 */
GdkTexture *
gdk_texture_new_for_surface (cairo_surface_t *surface)
{
  GdkTexture *texture;
  GBytes *bytes;

  g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
  g_return_val_if_fail (cairo_image_surface_get_width (surface) > 0, NULL);
  g_return_val_if_fail (cairo_image_surface_get_height (surface) > 0, NULL);

  bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
                                      cairo_image_surface_get_height (surface)
                                      * cairo_image_surface_get_stride (surface),
                                      (GDestroyNotify) cairo_surface_destroy,
                                      cairo_surface_reference (surface));
  
  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_CAIRO_FORMAT_ARGB32,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));

  g_bytes_unref (bytes);

  return texture;
}

/**
 * gdk_texture_new_for_pixbuf:
 * @pixbuf: a `GdkPixbuf`
 *
 * Creates a new texture object representing the `GdkPixbuf`.
 *
 * Returns: a new `GdkTexture`
 */
GdkTexture *
gdk_texture_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GdkTexture *texture;
  GBytes *bytes;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  bytes = g_bytes_new_with_free_func (gdk_pixbuf_get_pixels (pixbuf),
                                      gdk_pixbuf_get_height (pixbuf)
                                      * gdk_pixbuf_get_rowstride (pixbuf),
                                      g_object_unref,
                                      g_object_ref (pixbuf));
  texture = gdk_memory_texture_new (gdk_pixbuf_get_width (pixbuf),
                                    gdk_pixbuf_get_height (pixbuf),
                                    gdk_pixbuf_get_has_alpha (pixbuf)
                                    ? GDK_MEMORY_GDK_PIXBUF_ALPHA
                                    : GDK_MEMORY_GDK_PIXBUF_OPAQUE,
                                    bytes,
                                    gdk_pixbuf_get_rowstride (pixbuf));

  g_bytes_unref (bytes);

  return texture;
}

/**
 * gdk_texture_new_from_resource:
 * @resource_path: the path of the resource file
 *
 * Creates a new texture by loading an image from a resource.
 *
 * The file format is detected automatically. The supported formats
 * are PNG and JPEG, though more formats might be available.
 *
 * It is a fatal error if @resource_path does not specify a valid
 * image resource and the program will abort if that happens.
 * If you are unsure about the validity of a resource, use
 * [ctor@Gdk.Texture.new_from_file] to load it.
 *
 * Return value: A newly-created `GdkTexture`
 */
GdkTexture *
gdk_texture_new_from_resource (const char *resource_path)
{
  GError *error = NULL;
  GdkTexture *texture;
  GdkPixbuf *pixbuf;

  g_return_val_if_fail (resource_path != NULL, NULL);

  pixbuf = gdk_pixbuf_new_from_resource (resource_path, &error);
  if (pixbuf == NULL)
    g_error ("Resource path %s is not a valid image: %s", resource_path, error->message);

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

/**
 * gdk_texture_new_from_file:
 * @file: `GFile` to load
 * @error: Return location for an error
 *
 * Creates a new texture by loading an image from a file.
 *
 * The file format is detected automatically. The supported formats
 * are PNG and JPEG, though more formats might be available.
 *
 * If %NULL is returned, then @error will be set.
 *
 * Return value: A newly-created `GdkTexture` or %NULL if an error occurred.
 */
GdkTexture *
gdk_texture_new_from_file (GFile   *file,
                           GError **error)
{
  GdkTexture *texture;
  GdkPixbuf *pixbuf;
  GInputStream *stream;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  if (stream == NULL)
    return NULL;

  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, error);
  g_object_unref (stream);
  if (pixbuf == NULL)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

/**
 * gdk_texture_get_width: (attributes org.gtk.Method.get_property=width)
 * @texture: a `GdkTexture`
 *
 * Returns the width of @texture, in pixels.
 *
 * Returns: the width of the `GdkTexture`
 */
int
gdk_texture_get_width (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);

  return texture->width;
}

/**
 * gdk_texture_get_height: (attributes org.gtk.Method.get_property=height)
 * @texture: a `GdkTexture`
 *
 * Returns the height of the @texture, in pixels.
 *
 * Returns: the height of the `GdkTexture`
 */
int
gdk_texture_get_height (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);

  return texture->height;
}

cairo_surface_t *
gdk_texture_download_surface (GdkTexture *texture)
{
  cairo_surface_t *surface;
  cairo_status_t surface_status;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        texture->width, texture->height);

  surface_status = cairo_surface_status (surface);
  if (surface_status != CAIRO_STATUS_SUCCESS)
    g_warning ("%s: surface error: %s", __FUNCTION__,
               cairo_status_to_string (surface_status));

  gdk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  cairo_surface_mark_dirty (surface);

  return surface;
}

void
gdk_texture_download_area (GdkTexture         *texture,
                           const GdkRectangle *area,
                           guchar             *data,
                           gsize               stride)
{
  g_assert (area->x >= 0);
  g_assert (area->y >= 0);
  g_assert (area->x + area->width <= texture->width);
  g_assert (area->y + area->height <= texture->height);

  GDK_TEXTURE_GET_CLASS (texture)->download (texture, area, data, stride);
}

/**
 * gdk_texture_download:
 * @texture: a `GdkTexture`
 * @data: (array): pointer to enough memory to be filled with the
 *     downloaded data of @texture
 * @stride: rowstride in bytes
 *
 * Downloads the @texture into local memory.
 *
 * This may be an expensive operation, as the actual texture data
 * may reside on a GPU or on a remote display server.
 *
 * The data format of the downloaded data is equivalent to
 * %CAIRO_FORMAT_ARGB32, so every downloaded pixel requires
 * 4 bytes of memory.
 *
 * Downloading a texture into a Cairo image surface:
 * ```c
 * surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
 *                                       gdk_texture_get_width (texture),
 *                                       gdk_texture_get_height (texture));
 * gdk_texture_download (texture,
 *                       cairo_image_surface_get_data (surface),
 *                       cairo_image_surface_get_stride (surface));
 * cairo_surface_mark_dirty (surface);
 * ```
 */
void
gdk_texture_download (GdkTexture *texture,
                      guchar     *data,
                      gsize       stride)
{
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (data != NULL);
  g_return_if_fail (stride >= gdk_texture_get_width (texture) * 4);

  gdk_texture_download_area (texture,
                             &(GdkRectangle) { 0, 0, texture->width, texture->height },
                             data,
                             stride);
}

gboolean
gdk_texture_set_render_data (GdkTexture     *self,
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
gdk_texture_clear_render_data (GdkTexture *self)
{
  if (self->render_notify)
    self->render_notify (self->render_data);

  self->render_key = NULL;
  self->render_data = NULL;
  self->render_notify = NULL;
}

gpointer
gdk_texture_get_render_data (GdkTexture  *self,
                             gpointer     key)
{
  if (self->render_key != key)
    return NULL;

  return self->render_data;
}

/**
 * gdk_texture_save_to_png:
 * @texture: a `GdkTexture`
 * @filename: the filename to store to
 *
 * Store the given @texture to the @filename as a PNG file.
 *
 * This is a utility function intended for debugging and testing.
 * If you want more control over formats, proper error handling or
 * want to store to a `GFile` or other location, you might want to
 * look into using the gdk-pixbuf library.
 *
 * Returns: %TRUE if saving succeeded, %FALSE on failure.
 */
gboolean
gdk_texture_save_to_png (GdkTexture *texture,
                         const char *filename)
{
  cairo_surface_t *surface;
  cairo_status_t status;
  gboolean result;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gdk_texture_get_width (texture),
                                        gdk_texture_get_height (texture));
  gdk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  cairo_surface_mark_dirty (surface);

  status = cairo_surface_write_to_png (surface, filename);

  if (status != CAIRO_STATUS_SUCCESS ||
      cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    result = FALSE;
  else
    result = TRUE;

  cairo_surface_destroy (surface);

  return result;
}
