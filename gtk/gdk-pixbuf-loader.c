/* GdkPixbuf library - Main header file
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
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

#include <string.h>

#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-loader.h"
#include "gdk-pixbuf-io.h"

#include "gtksignal.h"

enum {
  AREA_UPDATED,
  AREA_PREPARED,
  FRAME_DONE,
  ANIMATION_DONE,
  CLOSED,
  LAST_SIGNAL
};


static void gdk_pixbuf_loader_class_init    (GdkPixbufLoaderClass   *klass);
static void gdk_pixbuf_loader_init          (GdkPixbufLoader        *loader);
static void gdk_pixbuf_loader_destroy       (GtkObject              *loader);
static void gdk_pixbuf_loader_finalize      (GObject                *loader);

static gpointer parent_class = NULL;
static guint    pixbuf_loader_signals[LAST_SIGNAL] = { 0 };


/* Internal data */

#define LOADER_HEADER_SIZE 128

typedef struct
{
  GdkPixbuf *pixbuf;
  GdkPixbufAnimation *animation;
  gboolean closed;
  guchar header_buf[LOADER_HEADER_SIZE];
  gint header_buf_offset;
  GdkPixbufModule *image_module;
  gpointer context;
} GdkPixbufLoaderPrivate;


/**
 * gdk_pixbuf_loader_get_type:
 * @void:
 *
 * Registers the #GdkPixubfLoader class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GdkPixbufLoader class.
 **/
GtkType
gdk_pixbuf_loader_get_type (void)
{
  static GtkType loader_type = 0;
  
  if (!loader_type)
    {
      static const GtkTypeInfo loader_info = {
	"GdkPixbufLoader",
	sizeof (GdkPixbufLoader),
	sizeof (GdkPixbufLoaderClass),
	(GtkClassInitFunc) gdk_pixbuf_loader_class_init,
	(GtkObjectInitFunc) gdk_pixbuf_loader_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };
      
      loader_type = gtk_type_unique (GTK_TYPE_OBJECT, &loader_info);
    }
  
  return loader_type;
}

static void
gdk_pixbuf_loader_class_init (GdkPixbufLoaderClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass *) class;
  gobject_class = (GObjectClass *) class;
  
  parent_class = gtk_type_class (GTK_TYPE_OBJECT);
  
  pixbuf_loader_signals[AREA_PREPARED] =
    gtk_signal_new ("area_prepared",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, area_prepared),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  pixbuf_loader_signals[AREA_UPDATED] =
    gtk_signal_new ("area_updated",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, area_updated),
		    gtk_marshal_VOID__INT_INT_INT_INT,
		    GTK_TYPE_NONE, 4,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT);
  
  pixbuf_loader_signals[FRAME_DONE] =
    gtk_signal_new ("frame_done",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, frame_done),
		    gtk_marshal_VOID__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);
  
  pixbuf_loader_signals[ANIMATION_DONE] =
    gtk_signal_new ("animation_done",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, animation_done),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  pixbuf_loader_signals[CLOSED] =
    gtk_signal_new ("closed",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, closed),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  gtk_object_class_add_signals (object_class, pixbuf_loader_signals, LAST_SIGNAL);
  
  object_class->destroy = gdk_pixbuf_loader_destroy;
  gobject_class->finalize = gdk_pixbuf_loader_finalize;
}

static void
gdk_pixbuf_loader_init (GdkPixbufLoader *loader)
{
  GdkPixbufLoaderPrivate *priv;
  
  priv = g_new0 (GdkPixbufLoaderPrivate, 1);
  loader->private = priv;
}

static void
gdk_pixbuf_loader_destroy (GtkObject *object)
{
  GdkPixbufLoader *loader;
  GdkPixbufLoaderPrivate *priv = NULL;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GDK_IS_PIXBUF_LOADER (object));
  
  loader = GDK_PIXBUF_LOADER (object);
  priv = loader->private;
  
  if (!priv->closed)
    gdk_pixbuf_loader_close (loader);
  
  if (priv->animation)
    gdk_pixbuf_animation_unref (priv->animation);
  if (priv->pixbuf)
    gdk_pixbuf_unref (priv->pixbuf);
  
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gdk_pixbuf_loader_finalize (GObject *object)
{
  GdkPixbufLoader *loader;
  GdkPixbufLoaderPrivate *priv = NULL;
  
  loader = GDK_PIXBUF_LOADER (object);
  priv = loader->private;
  
  g_free (priv);
  
  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_pixbuf_loader_prepare (GdkPixbuf *pixbuf,
			   gpointer   loader)
{
  GdkPixbufLoaderPrivate *priv = NULL;
  
  priv = GDK_PIXBUF_LOADER (loader)->private;
  gdk_pixbuf_ref (pixbuf);

  g_assert (priv->pixbuf == NULL);
  
  priv->pixbuf = pixbuf;
  gtk_signal_emit (GTK_OBJECT (loader), pixbuf_loader_signals[AREA_PREPARED]);
}

static void
gdk_pixbuf_loader_update (GdkPixbuf *pixbuf,
			  guint      x,
			  guint      y,
			  guint      width,
			  guint      height,
			  gpointer   loader)
{
  GdkPixbufLoaderPrivate *priv = NULL;
  
  priv = GDK_PIXBUF_LOADER (loader)->private;
  
  gtk_signal_emit (GTK_OBJECT (loader),
		   pixbuf_loader_signals[AREA_UPDATED],
		   x, y,
		   /* sanity check in here.  Defend against an errant loader */
		   MIN (width, gdk_pixbuf_get_width (priv->pixbuf)),
		   MIN (height, gdk_pixbuf_get_height (priv->pixbuf)));
}

static void
gdk_pixbuf_loader_frame_done (GdkPixbufFrame *frame,
			      gpointer        loader)
{
  GdkPixbufLoaderPrivate *priv = NULL;
  
  priv = GDK_PIXBUF_LOADER (loader)->private;
  
  priv->pixbuf = NULL;
  
  if (priv->animation == NULL)
    {
      priv->animation = g_object_new (GDK_TYPE_PIXBUF_ANIMATION, NULL);
      
      priv->animation->n_frames = 0;
      priv->animation->width  = gdk_pixbuf_get_width  (frame->pixbuf) + frame->x_offset;
      priv->animation->height = gdk_pixbuf_get_height (frame->pixbuf) + frame->y_offset;
    }
  else
    {
      int w, h;
      
      /* update bbox size */
      w = gdk_pixbuf_get_width (frame->pixbuf) + frame->x_offset;
      h = gdk_pixbuf_get_height (frame->pixbuf) + frame->y_offset;
      
      if (w > priv->animation->width) {
	priv->animation->width = w;
      }
      if (h > priv->animation->height) {
	priv->animation->height = h;
      }
    }
  
  priv->animation->frames = g_list_append (priv->animation->frames, frame);
  priv->animation->n_frames++;
  gtk_signal_emit (GTK_OBJECT (loader),
		   pixbuf_loader_signals[FRAME_DONE],
		   frame);
}

static void
gdk_pixbuf_loader_animation_done (GdkPixbuf *pixbuf,
				  gpointer   loader)
{
  GdkPixbufLoaderPrivate *priv = NULL;
  GdkPixbufFrame    *frame;
  GList *current = NULL;
  gint h, w;
  
  priv = GDK_PIXBUF_LOADER (loader)->private;
  priv->pixbuf = NULL;
  
  current = gdk_pixbuf_animation_get_frames (priv->animation);
  
  while (current)
    {
      frame = (GdkPixbufFrame *) current->data;
      
      /* update bbox size */
      w = gdk_pixbuf_get_width (frame->pixbuf) + frame->x_offset;
      h = gdk_pixbuf_get_height (frame->pixbuf) + frame->y_offset;
      
      if (w > priv->animation->width)
	priv->animation->width = w;
      if (h > priv->animation->height)
	priv->animation->height = h;
      current = current->next;
    }
  
  gtk_signal_emit (GTK_OBJECT (loader), pixbuf_loader_signals[ANIMATION_DONE]);
}

static gint
gdk_pixbuf_loader_load_module (GdkPixbufLoader *loader,
                               const char      *image_type,
                               GError         **error)
{
  GdkPixbufLoaderPrivate *priv = loader->private;

  if (image_type)
    {
      priv->image_module = gdk_pixbuf_get_named_module (image_type,
                                                        error);
    }
  else
    {
      g_return_val_if_fail (priv->header_buf_offset > 0, 0);
      priv->image_module = gdk_pixbuf_get_module (priv->header_buf,
                                                  priv->header_buf_offset,
                                                  NULL,
                                                  error);
    }
  
  if (priv->image_module == NULL)
    return 0;
  
  if (priv->image_module->module == NULL)
    if (!gdk_pixbuf_load_module (priv->image_module, error))
      return 0;
  
  if (priv->image_module->module == NULL)
    return 0;
  
  if ((priv->image_module->begin_load == NULL) ||
      (priv->image_module->stop_load == NULL) ||
      (priv->image_module->load_increment == NULL))
    {
      g_set_error (error,
                   GDK_PIXBUF_ERROR,
                   GDK_PIXBUF_ERROR_UNSUPPORTED_OPERATION,
                   _("Incremental loading of image type '%s' is not supported"),
                   image_type);

      return 0;
    }
  
  priv->context = priv->image_module->begin_load (gdk_pixbuf_loader_prepare,
						  gdk_pixbuf_loader_update,
						  gdk_pixbuf_loader_frame_done,
						  gdk_pixbuf_loader_animation_done,
						  loader,
                                                  error);
  
  if (priv->context == NULL)
    {
      /* Defense against broken loaders; DO NOT take this as a GError
       * example
       */
      if (error && *error == NULL)
        {
          g_warning ("Bug! loader '%s' didn't set an error on failure",
                     priv->image_module->module_name);
          g_set_error (error,
                       GDK_PIXBUF_ERROR,
                       GDK_PIXBUF_ERROR_FAILED,
                       _("Internal error: Image loader module '%s'"
                         " failed to begin loading an image, but didn't"
                         " give a reason for the failure"),
                       priv->image_module->module_name);

        }
      
      return 0;
    }
  
  if (priv->header_buf_offset
      && priv->image_module->load_increment (priv->context, priv->header_buf, priv->header_buf_offset, error))
    return priv->header_buf_offset;
  
  return 0;
}

static int
gdk_pixbuf_loader_eat_header_write (GdkPixbufLoader *loader,
				    const guchar    *buf,
				    gsize            count,
                                    GError         **error)
{
  gint n_bytes;
  GdkPixbufLoaderPrivate *priv = loader->private;
  
  n_bytes = MIN(LOADER_HEADER_SIZE - priv->header_buf_offset, count);
  memcpy (priv->header_buf + priv->header_buf_offset, buf, n_bytes);
  
  priv->header_buf_offset += n_bytes;
  
  if (priv->header_buf_offset >= LOADER_HEADER_SIZE)
    {
      if (gdk_pixbuf_loader_load_module (loader, NULL, error) == 0)
	return 0;
    }
  
  return n_bytes;
}

/**
 * gdk_pixbuf_loader_write:
 * @loader: A pixbuf loader.
 * @buf: Pointer to image data.
 * @count: Length of the @buf buffer in bytes.
 * @error: return location for errors
 *
 * This will cause a pixbuf loader to parse the next @count bytes of
 * an image.  It will return TRUE if the data was loaded successfully,
 * and FALSE if an error occurred.  In the latter case, the loader
 * will be closed, and will not accept further writes. If FALSE is
 * returned, @error will be set to an error from the #GDK_PIXBUF_ERROR
 * domain.
 *
 * Return value: #TRUE if the write was successful, or #FALSE if the loader
 * cannot parse the buffer.
 **/
gboolean
gdk_pixbuf_loader_write (GdkPixbufLoader *loader,
			 const guchar    *buf,
			 gsize            count,
                         GError         **error)
{
  GdkPixbufLoaderPrivate *priv;
  
  g_return_val_if_fail (loader != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), FALSE);
  
  g_return_val_if_fail (buf != NULL, FALSE);
  g_return_val_if_fail (count >= 0, FALSE);
  
  priv = loader->private;
  
  /* we expect it's not to be closed */
  g_return_val_if_fail (priv->closed == FALSE, FALSE);
  
  if (priv->image_module == NULL)
    {
      gint eaten;
      
      eaten = gdk_pixbuf_loader_eat_header_write(loader, buf, count, error);
      if (eaten <= 0)
	return FALSE;
      
      count -= eaten;
      buf += eaten;
    }
  
  if (count > 0 && priv->image_module->load_increment)
    {
      gboolean retval;
      retval = priv->image_module->load_increment (priv->context, buf, count,
                                                   error);
      if (!retval && error && *error == NULL)
        {
          /* Fix up busted image loader */
          g_warning ("Bug! loader '%s' didn't set an error on failure",
                     priv->image_module->module_name);
          g_set_error (error,
                       GDK_PIXBUF_ERROR,
                       GDK_PIXBUF_ERROR_FAILED,
                       _("Internal error: Image loader module '%s'"
                         " failed to begin loading an image, but didn't"
                         " give a reason for the failure"),
                       priv->image_module->module_name);
        }

      return retval;
    }
      
  return TRUE;
}

/**
 * gdk_pixbuf_loader_new:
 *
 * Creates a new pixbuf loader object.
 *
 * Return value: A newly-created pixbuf loader.
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new (void)
{
  return g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);
}

/**
 * gdk_pixbuf_loader_new_with_type:
 *
 * Creates a new pixbuf loader object.
 *
 * Return value: A newly-created pixbuf loader.
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new_with_type (const char *image_type,
                                 GError    **error)
{
  GdkPixbufLoader *retval;
  GError *tmp;
  
  retval = g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);

  tmp = NULL;
  gdk_pixbuf_loader_load_module(retval, image_type, &tmp);
  if (tmp != NULL)
    {
      g_propagate_error (error, tmp);
      g_object_unref (G_OBJECT (retval));
      return NULL;
    }

  return retval;
}

/**
 * gdk_pixbuf_loader_get_pixbuf:
 * @loader: A pixbuf loader.
 *
 * Queries the GdkPixbuf that a pixbuf loader is currently creating.
 * In general it only makes sense to call this function afer the
 * "area_prepared" signal has been emitted by the loader; this means
 * that enough data has been read to know the size of the image that
 * will be allocated.  If the loader has not received enough data via
 * gdk_pixbuf_loader_write(), then this function returns NULL.  The
 * returned pixbuf will be the same in all future calls to the loader,
 * so simply calling gdk_pixbuf_ref() should be sufficient to continue
 * using it.  Additionally, if the loader is an animation, it will
 * return the first frame of the animation.
 * 
 * Return value: The GdkPixbuf that the loader is creating, or NULL if not
 * enough data has been read to determine how to create the image buffer.
 **/
GdkPixbuf *
gdk_pixbuf_loader_get_pixbuf (GdkPixbufLoader *loader)
{
  GdkPixbufLoaderPrivate *priv;
  
  g_return_val_if_fail (loader != NULL, NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
  priv = loader->private;

  if (priv->animation)
    {
      GList *list;
      
      list = gdk_pixbuf_animation_get_frames (priv->animation);
      if (list != NULL)
        {
          GdkPixbufFrame *frame = list->data;
          
          return gdk_pixbuf_frame_get_pixbuf (frame);
        }
    }
  
  return priv->pixbuf;
}

/**
 * gdk_pixbuf_loader_get_animation:
 * @loader: A pixbuf loader
 *
 * Queries the GdkPixbufAnimation that a pixbuf loader is currently creating.
 * In general it only makes sense to call this function afer the "area_prepared"
 * signal has been emitted by the loader.  If the image is not an animation,
 * then it will return NULL.
 *
 * Return value: The GdkPixbufAnimation that the loader is loading, or NULL if
 not enough data has been read to determine the information.
**/
GdkPixbufAnimation *
gdk_pixbuf_loader_get_animation (GdkPixbufLoader *loader)
{
  GdkPixbufLoaderPrivate *priv;
  
  g_return_val_if_fail (loader != NULL, NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
  priv = loader->private;
  
  return priv->animation;
}

/**
 * gdk_pixbuf_loader_close:
 * @loader: A pixbuf loader.
 *
 * Informs a pixbuf loader that no further writes with gdk_pixbuf_load_write()
 * will occur, so that it can free its internal loading structures.
 **/
void
gdk_pixbuf_loader_close (GdkPixbufLoader *loader)
{
  GdkPixbufLoaderPrivate *priv;
  
  g_return_if_fail (loader != NULL);
  g_return_if_fail (GDK_IS_PIXBUF_LOADER (loader));
  
  priv = loader->private;
  
  /* we expect it's not closed */
  g_return_if_fail (priv->closed == FALSE);
  
  /* We have less the 128 bytes in the image.  Flush it, and keep going. */
  if (priv->image_module == NULL)
    gdk_pixbuf_loader_load_module (loader, NULL, NULL);
  
  if (priv->image_module && priv->image_module->stop_load)
    priv->image_module->stop_load (priv->context);
  
  priv->closed = TRUE;
  
  gtk_signal_emit (GTK_OBJECT (loader), pixbuf_loader_signals[CLOSED]);
}
