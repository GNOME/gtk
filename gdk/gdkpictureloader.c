/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Benjamin Otte <otte@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkpictureloader.h"

#include "gdkintl.h"
#include "gdkinternals.h"
#include "gdkpixbuf.h"

typedef struct _GdkPictureAsyncLoad GdkPictureAsyncLoad;

struct _GdkPictureLoaderPrivate {
  GdkPictureAsyncLoad *load;
  GdkPicture *picture;
  GError *error;
};

struct _GdkPictureAsyncLoad {
  GdkPictureLoader *loader;
  GdkPixbufLoader *pixbuf_loader;
  guchar buffer[4096];

  int io_priority;
  GCancellable *cancellable;
  GAsyncReadyCallback callback;
  gpointer user_data;
};

G_DEFINE_TYPE (GdkPictureLoader, gdk_picture_loader, GDK_TYPE_PICTURE)

enum {
  PROP_0,
  PROP_PICTURE,
  PROP_ERROR,
};

/**
 * SECTION:gdkpictureloader
 * @Short_description: Loading images into Pictures
 * @Title: GdkPictureLoader
 * @See_also: #GdkPicture
 *
 * A #GdkPictureLoader is an implementation of #GdkPicture that can load
 * images from a stream. It takes care of figuring out the format,
 * decoding the image and error handling.
 */

static void
gdk_picture_loader_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkPictureLoader *loader = GDK_PICTURE_LOADER (object);
  GdkPictureLoaderPrivate *priv = loader->priv;

  switch (prop_id)
    {
    case PROP_PICTURE:
      g_value_set_object (value, priv->picture);
      break;
    case PROP_ERROR:
      g_value_set_boxed (value, priv->error);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_picture_loader_draw (GdkPicture *picture,
                         cairo_t    *cr)
{
  GdkPictureLoader *loader = GDK_PICTURE_LOADER (picture);
  GdkPictureLoaderPrivate *priv = loader->priv;

  if (priv->error)
    {
      /* FIXME: Draw broken image icon */
      cairo_set_source_rgb (cr, 1, 0, 0);
      cairo_rectangle (cr, 0, 0,
                       gdk_picture_get_width (picture),
                       gdk_picture_get_height (picture));
      cairo_fill (cr);
      return;
    }

  if (priv->picture)
    {
      gdk_picture_draw (priv->picture, cr);
      return;
    }

  if (priv->load)
    {
      /* FIXME: Draw loading icon */
      cairo_set_source_rgb (cr, 0, 0, 1);
      cairo_rectangle (cr, 0, 0,
                       gdk_picture_get_width (picture),
                       gdk_picture_get_height (picture));
      cairo_fill (cr);
      return;
    }

  /* no load happening, size should be empty */
  g_assert (gdk_picture_get_width (picture) == 0);
  g_assert (gdk_picture_get_height (picture) == 0);

  return;
}

static void
gdk_picture_loader_class_init (GdkPictureLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->get_property = gdk_picture_loader_get_property;

  picture_class->draw = gdk_picture_loader_draw;

  /**
   * GdkPictureLoader:picture:
   *
   * The picture that is loading or %NULL if loading hasn't processed
   * enough or no the loader is in an error.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_PICTURE,
                                   g_param_spec_object ("picture",
                                                        P_("picture"),
                                                        P_("the loading picture"),
                                                        GDK_TYPE_PICTURE,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GdkPictureLoader:error:
   *
   * The error that happened during loading or %NULL if no error happened yet.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_ERROR,
                                   g_param_spec_boxed ("error",
                                                       P_("error"),
                                                       P_("error that happened during load"),
                                                       G_TYPE_ERROR,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (GdkPictureLoaderPrivate));
}

static void
gdk_picture_loader_init (GdkPictureLoader *loader)
{
  loader->priv = G_TYPE_INSTANCE_GET_PRIVATE (loader,
                                              GDK_TYPE_PICTURE_LOADER,
                                              GdkPictureLoaderPrivate);
}

/**
 * gdk_picture_loader_new:
 *
 * Creates a new #GdkPictureLoader for loading images.
 *
 * Returns: a new picture
 **/
GdkPicture *
gdk_picture_loader_new (void)
{
  return g_object_new (GDK_TYPE_PICTURE_LOADER, NULL);
}

/**
 * gdk_picture_loader_get_error:
 * @loader: the loader to check
 *
 * If there was an error while loading the image, it can be queried
 * with this function. Otherwise, %NULL is returned.
 *
 * Returns: The error that @loader is in or %NULL if none
 **/
const GError *
gdk_picture_loader_get_error (GdkPictureLoader *loader)
{
  g_return_val_if_fail (GDK_IS_PICTURE_LOADER (loader), NULL);

  return loader->priv->error;
}

/**
 * gdk_picture_loader_get_picture:
 * @loader: the loader to get the picture from
 *
 * Gets the picture that is currently loading or has been loaded. If
 * there is an error or the load has not progressed enough
 *
 * Returns: (transfer none) (allow-none): a %GdkPicture or %NULL.
 **/
GdkPicture *
gdk_picture_loader_get_picture (GdkPictureLoader *loader)
{
  g_return_val_if_fail (GDK_IS_PICTURE_LOADER (loader), NULL);

  return loader->priv->picture;
}

static void
gdk_picture_loader_reset (GdkPictureLoader *loader)
{
  GdkPictureLoaderPrivate *priv = loader->priv;

  if (priv->load) {
    if (priv->load->loader)
    g_object_unref (priv->load->loader);
    priv->load->loader = NULL;
    priv->load = NULL;
  }
  if (priv->error) {
    g_error_free (priv->error);
    priv->error = NULL;
    g_object_notify (G_OBJECT (loader), "error");
  }
  if (priv->picture) {
    g_object_unref (priv->picture);
    priv->picture = NULL;
    g_object_notify (G_OBJECT (loader), "picture");
  }

  gdk_picture_resized (GDK_PICTURE (loader), 0, 0);
}

static void
gdk_picture_loader_handle_error (GdkPictureLoader *loader,
                                 GError **         error)
{
  GdkPictureLoaderPrivate *priv = loader->priv;

  g_assert (priv->error);

  if (error)
    *error = g_error_copy (priv->error);

  g_object_notify (G_OBJECT (loader), "error");
}

static void
gdk_picture_loader_size_prepared (GdkPixbufLoader  *loader,
                                  int               width,
                                  int               height,
                                  GdkPictureLoader *picture)
{
  gdk_picture_resized (GDK_PICTURE (picture), width, height);
}

static void
gdk_picture_loader_area_prepared (GdkPixbufLoader  *loader,
                                  GdkPictureLoader *picture)
{
  GdkPictureLoaderPrivate *priv = picture->priv;

  priv->picture = gdk_pixbuf_picture_new (gdk_pixbuf_loader_get_pixbuf (loader));

  g_assert (gdk_picture_get_width (priv->picture) == gdk_picture_get_width (GDK_PICTURE (picture)));
  g_assert (gdk_picture_get_height (priv->picture) == gdk_picture_get_height (GDK_PICTURE (picture)));

  g_object_notify (G_OBJECT (picture), "picture");
}

static void
gdk_picture_loader_area_updated (GdkPixbufLoader  *loader,
                                 int               x,
                                 int               y,
                                 int               width,
                                 int               height,
                                 GdkPictureLoader *picture)
{
  cairo_rectangle_int_t rect = { x, y, width, height };

  gdk_picture_changed_rect (picture->priv->picture, &rect);
  gdk_picture_changed_rect (GDK_PICTURE (picture), &rect);
}

static GdkPixbufLoader *
gdk_picture_loader_create_loader (GdkPictureLoader *loader)
{
  GdkPixbufLoader *pixbuf_loader;

  pixbuf_loader = gdk_pixbuf_loader_new ();
  g_signal_connect (pixbuf_loader,
                    "size-prepared",
                    G_CALLBACK (gdk_picture_loader_size_prepared),
                    loader);
  g_signal_connect (pixbuf_loader,
                    "area-prepared",
                    G_CALLBACK (gdk_picture_loader_area_prepared),
                    loader);
  g_signal_connect (pixbuf_loader,
                    "area-updated",
                    G_CALLBACK (gdk_picture_loader_area_updated),
                    loader);

  return pixbuf_loader;
}

void
gdk_picture_loader_load_from_stream (GdkPictureLoader *loader,
                                     GInputStream *    stream,
                                     GCancellable *    cancellable,
                                     GError **         error)
{
  GdkPictureLoaderPrivate *priv;
  GdkPixbufLoader *pixbuf_loader;
  guchar buffer[4096];
  gssize bytes_read;

  g_return_if_fail (GDK_IS_PICTURE_LOADER (loader));
  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  priv = loader->priv;
  pixbuf_loader = gdk_picture_loader_create_loader (loader);

  do
    {
      bytes_read = g_input_stream_read (stream,
                                        buffer,
                                        sizeof (buffer),
                                        cancellable,
                                        &priv->error);

      if (bytes_read > 0)
        {
          if (!gdk_pixbuf_loader_write (pixbuf_loader,
                                        buffer,
                                        bytes_read,
                                        &priv->error))
            bytes_read = -1;
        }
      else if (bytes_read == 0)
        {
          if (!gdk_pixbuf_loader_close (pixbuf_loader,
                                        &priv->error))
            bytes_read = -1;
        }

      if (bytes_read < 0)
        gdk_picture_loader_handle_error (loader, error);
    }
  while (bytes_read > 0);

  g_object_unref (pixbuf_loader);
}

void
gdk_picture_loader_load_from_file (GdkPictureLoader *loader,
                                   GFile *           file,
                                   GCancellable *    cancellable,
                                   GError **         error)
{
  GdkPictureLoaderPrivate *priv;
  GFileInputStream *stream;

  g_return_if_fail (GDK_IS_PICTURE_LOADER (loader));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  priv = loader->priv;
  gdk_picture_loader_reset (loader);

  stream = g_file_read (file, cancellable, &priv->error);
  if (stream == NULL)
    {
      gdk_picture_loader_handle_error (loader, error);
      return;
    }

  gdk_picture_loader_load_from_stream (loader,
                                       G_INPUT_STREAM (stream),
                                       cancellable,
                                       error);
  g_object_unref (stream);
}

void
gdk_picture_loader_load_from_filename (GdkPictureLoader *loader,
                                       const char *      filename,
                                       GCancellable *    cancellable,
                                       GError **         error)
{
  GFile *file;

  g_return_if_fail (GDK_IS_PICTURE_LOADER (loader));
  g_return_if_fail (filename != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  file = g_file_new_for_path (filename);
  gdk_picture_loader_load_from_file (loader, file, cancellable, error);
  g_object_unref (file);
}

static GdkPictureAsyncLoad *
gdk_picture_async_load_new (GdkPictureLoader *  loader,
                            int                 io_priority,
                            GCancellable *      cancellable,
                            GAsyncReadyCallback callback,
                            gpointer            user_data)
{
  GdkPictureAsyncLoad *data = g_slice_new0 (GdkPictureAsyncLoad);

  data->loader = g_object_ref (loader);
  data->io_priority = io_priority;
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  data->callback = callback;
  data->user_data = user_data;
  data->pixbuf_loader = gdk_picture_loader_create_loader (loader);

  loader->priv->load = data;

  return data;
}

static void
gdk_picture_async_load_destroy (GdkPictureAsyncLoad *data)
{
  if (data->loader && data->loader->priv->load == data)
    data->loader->priv->load = NULL;

  if (data->callback)
    data->callback (G_OBJECT (data->loader), NULL, data->user_data);

  g_object_unref (data->loader);
  if (data->cancellable)
    g_object_unref (data->cancellable);
  g_object_unref (data->pixbuf_loader);

  g_slice_free (GdkPictureAsyncLoad, data);
}

static void
gdk_picture_async_load_handle_error (GdkPictureAsyncLoad *data)
{
  gdk_picture_loader_handle_error (data->loader, NULL);
  gdk_picture_async_load_destroy (data);
}

static gboolean
gdk_picture_load_check_active (GdkPictureAsyncLoad *data)
{
  return data != NULL && data->loader != NULL;
}

static void
gdk_picture_loader_close_callback (GObject *     object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (object);
  GdkPictureAsyncLoad *data = user_data;
  GdkPictureLoaderPrivate *priv;
  
  /* Do we need to call this? */
  if (!gdk_picture_load_check_active (data))
    {
      g_input_stream_close_finish (stream, res, NULL);
      return;
    }
      
  priv = data->loader->priv;

  if (!g_input_stream_close_finish (stream, res, &priv->error))
    {
      gdk_picture_async_load_handle_error (data);
      return;
    }

  gdk_picture_async_load_destroy (data);
}

static void
gdk_picture_loader_read_callback (GObject *     object,
                                  GAsyncResult *res,
                                  gpointer      user_data)
{
  GdkPictureAsyncLoad *data = user_data;
  GInputStream *stream = G_INPUT_STREAM (object);
  
  if (gdk_picture_load_check_active (data))
    {
      GdkPictureLoaderPrivate *priv = data->loader->priv;
      gssize bytes_read;

      bytes_read = g_input_stream_read_finish (stream, res, &priv->error);

      if (bytes_read > 0)
        {
          if (gdk_pixbuf_loader_write (data->pixbuf_loader,
                                       data->buffer,
                                       bytes_read,
                                       &priv->error))
            {
              g_input_stream_read_async (stream,
                                         data->buffer,
                                         sizeof (data->buffer),
                                         data->io_priority,
                                         data->cancellable,
                                         gdk_picture_loader_read_callback,
                                         data);
              return;
            }
          bytes_read = -1;
        }
      else if (bytes_read == 0)
        {
          if (!gdk_pixbuf_loader_close (data->pixbuf_loader,
                                        &priv->error))
                bytes_read = -1;
            }

      if (bytes_read < 0)
        {
          g_input_stream_close_async (stream,
                                      data->io_priority,
                                      data->cancellable,
                                      gdk_picture_loader_close_callback,
                                      NULL);
          gdk_picture_async_load_handle_error (data);
          return;
        }
  }

  g_input_stream_close_async (stream,
                              data->io_priority,
                              data->cancellable,
                              gdk_picture_loader_close_callback,
                              data);
}

static void
gdk_picture_async_load_start (GdkPictureAsyncLoad *data,
                              GInputStream        *stream)
{
  g_input_stream_read_async (stream,
                             data->buffer,
                             sizeof (data->buffer),
                             data->io_priority,
                             data->cancellable,
                             gdk_picture_loader_read_callback,
                             data);
}

void
gdk_picture_loader_load_from_stream_async (GdkPictureLoader *  loader,
                                           GInputStream *      stream,
                                           int                 io_priority,
                                           GCancellable *      cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data)
{
  GdkPictureAsyncLoad *data;
  GdkPictureLoaderPrivate *priv;

  g_return_if_fail (GDK_IS_PICTURE_LOADER (loader));
  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  priv = loader->priv;
  gdk_picture_loader_reset (loader);

  data = gdk_picture_async_load_new (loader,
                                     io_priority,
                                     cancellable,
                                     callback,
                                     user_data);

  gdk_picture_async_load_start (data, stream);
}

static void
gdk_picture_loader_file_read_callback (GObject *     object,
                                       GAsyncResult *res,
                                       gpointer      user_data)
{
  GdkPictureAsyncLoad *data = user_data;
  GFile *file = G_FILE (object);
  GdkPictureLoaderPrivate *priv = data->loader->priv;
  GInputStream *stream;

  stream = G_INPUT_STREAM (g_file_read_finish (file, res, &priv->error));
  if (stream == NULL)
    {
      gdk_picture_async_load_handle_error (data);
      return;
    }

  if (gdk_picture_load_check_active (data))
    gdk_picture_async_load_start (data, stream);

  g_object_unref (stream);
}

void
gdk_picture_loader_load_from_file_async (GdkPictureLoader *  loader,
                                         GFile *             file,
                                         int                 io_priority,
                                         GCancellable *      cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer            user_data)
{
  GdkPictureAsyncLoad *load;
  GdkPictureLoaderPrivate *priv;

  g_return_if_fail (GDK_IS_PICTURE_LOADER (loader));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  priv = loader->priv;
  gdk_picture_loader_reset (loader);

  load = gdk_picture_async_load_new (loader,
                                     io_priority,
                                     cancellable,
                                     callback,
                                     user_data);

  g_file_read_async (file,
                     io_priority,
                     cancellable,
                     gdk_picture_loader_file_read_callback,
                     load);
}

void
gdk_picture_loader_load_from_filename_async (GdkPictureLoader *  loader,
                                             const char *        filename,
                                             int                 io_priority,
                                             GCancellable *      cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer            user_data)
{
  GFile *file;

  g_return_if_fail (GDK_IS_PICTURE_LOADER (loader));
  g_return_if_fail (filename != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  file = g_file_new_for_path (filename);
  gdk_picture_loader_load_from_file_async (loader,
                                           file,
                                           io_priority,
                                           cancellable,
                                           callback,
                                           user_data);
  g_object_unref (file);
}
