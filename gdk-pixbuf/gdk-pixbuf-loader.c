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
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gdk-pixbuf-loader.h"
#include "gdk-pixbuf-io.h"

enum {
	AREA_UPDATED,
	AREA_PREPARED,
	CLOSED,
	LAST_SIGNAL
};

static GtkObjectClass *parent_class;

static void gdk_pixbuf_loader_class_init    (GdkPixbufLoaderClass   *klass);
static void gdk_pixbuf_loader_init          (GdkPixbufLoader        *loader);
static void gdk_pixbuf_loader_destroy       (GtkObject              *loader);
static void gdk_pixbuf_loader_finalize      (GtkObject              *loader);


/* Internal data */
typedef struct _GdkPixbufLoaderPrivate GdkPixbufLoaderPrivate;
struct _GdkPixbufLoaderPrivate
{
	GdkPixbuf *pixbuf;
	gboolean closed;
	gchar buf[128];
	gint buf_offset;
	ModuleType *image_module;
	gpointer context;
};
static guint pixbuf_loader_signals[LAST_SIGNAL] = { 0 };


/* our marshaller */
typedef void (*GtkSignal_NONE__INT_INT_INT_INT) (GtkObject * object,
							 gint arg1,
							 gpointer arg2,
							 gint arg3,
							 gint arg4,
							 gint arg5,
							 gpointer user_data);
void
gtk_marshal_NONE__INT_INT_INT_INT (GtkObject * object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg * args)
{
  GtkSignal_NONE__INT_INT_INT_INT rfunc;
  rfunc = (GtkSignal_NONE__INT_INT_INT_INT) func;
  (*rfunc) (object,
	    GTK_VALUE_INT (args[0]),
	    GTK_VALUE_INT (args[1]),
	    GTK_VALUE_INT (args[2]),
	    GTK_VALUE_INT (args[3]),
	    func_data);
}

GtkType
gdk_pixbuf_loader_get_type (void)
{
	static GtkType loader_type = 0;

	if (!loader_type) {
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
gdk_pixbuf_loader_class_init (GdkPixbufLoaderClass *klass)
{
	parent_class = GTK_OBJECT_CLASS (klass);

	pixbuf_loader_signals[AREA_PREPARED] =
		gtk_signal_new ("area_prepared",
				GTK_RUN_LAST,
				parent_class->type,
				GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, area_prepared),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	pixbuf_loader_signals[AREA_UPDATED] =
		gtk_signal_new ("area_updated",
				GTK_RUN_LAST,
				parent_class->type,
				GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, area_updated),
				gtk_marshal_NONE__INT_INT_INT_INT,
				GTK_TYPE_NONE, 4, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);

	pixbuf_loader_signals[CLOSED] =
		gtk_signal_new ("closed",
				GTK_RUN_LAST,
				parent_class->type,
				GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, closed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (parent_class, pixbuf_loader_signals, LAST_SIGNAL);

	parent_class->destroy = gdk_pixbuf_loader_destroy;
	parent_class->finalize = gdk_pixbuf_loader_finalize;
}

static void
gdk_pixbuf_loader_init (GdkPixbufLoader *loader)
{
	GdkPixbufLoaderPrivate *priv;

	priv = g_new (GdkPixbufLoaderPrivate, 1);
	loader->private = priv;

	priv->pixbuf = NULL;
	priv->closed = FALSE;
	priv->buf_offset = 0;
}

static void
gdk_pixbuf_loader_destroy (GtkObject *loader)
{
	GdkPixbufLoaderPrivate *priv = NULL;

	priv = GDK_PIXBUF_LOADER (loader)->private;

	/* We want to close it if it's not already closed */
	if (priv->closed)
		gdk_pixbuf_loader_close (GDK_PIXBUF_LOADER (loader));

	if (priv->pixbuf)
		gdk_pixbuf_unref (priv->pixbuf);
}

static void
gdk_pixbuf_loader_finalize (GtkObject *loader)
{
	GdkPixbufLoaderPrivate *priv = NULL;

	priv = GDK_PIXBUF_LOADER (loader)->private;
	g_free (priv);
}

static void
gdk_pixbuf_loader_prepare (GdkPixbuf *pixbuf, gpointer loader)
{
	GdkPixbufLoaderPrivate *priv = NULL;

	priv = GDK_PIXBUF_LOADER (loader)->private;
	gdk_pixbuf_ref (pixbuf);
	g_assert (priv->pixbuf == NULL);

	priv->pixbuf = pixbuf;
	gtk_signal_emit (GTK_OBJECT (loader), druid_page_signals[AREA_PREPARED]);
}

/* Public functions */
GtkObject *
gdk_pixbuf_loader_new (void)
{
	GdkPixbufLoader *loader;

	loader = gtk_type_new (gdk_pixbuf_loader_get_type ());

	return GTK_OBJECT (loader);
}

/**
 * gdk_pixbuf_loader_write:
 * @loader: A loader.
 * @buf: The image data.
 * @count: The length of @buf in bytes.
 *
 * This will load the next @size bytes of the image.  It will return TRUE if the
 * data was loaded successfully, and FALSE if an error occurred. In this case,
 * the loader will be closed, and will not accept further writes.
 *
 * Return value: Returns TRUE if the write was successful -- FALSE if the loader
 * cannot parse the buf.
 **/
gboolean
gdk_pixbuf_loader_write (GdkPixbufLoader *loader, gchar *buf, gint count)
{
	GdkPixbufLoaderPrivate *priv;

	g_return_val_if_fail (loader != NULL, FALSE);
	g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);
	g_return_val_if_fail (count >= 0, FALSE);

	priv = loader->private;

	/* we expect it's not to be closed */
	g_return_val_if_fail (priv->closed == FALSE, FALSE);

	if (priv->image_module == NULL) {
		gboolean retval = TRUE;

		g_print ("buf_offset:%d:\n", priv->buf_offset);
		memcpy (priv->buf + priv->buf_offset,
			buf,
			(priv->buf_offset + count) > 128 ? 128 - priv->buf_offset : count);
		if ((priv->buf_offset + count) >= 128) {
			/* We have enough data to start doing something with the image */
			priv->image_module = gdk_pixbuf_get_module (priv->buf, 128);
			if (priv->image_module == NULL) {
				return FALSE;
			} else if ((priv->image_module->begin_load == NULL) ||
				   (priv->image_module->begin_load == NULL) ||
				   (priv->image_module->begin_load == NULL) ||
				   (priv->image_module->begin_load == NULL)) {
				g_warning ("module %s does not support incremental loading.\n", priv->image_module->module_name);
				return FALSE;
			} else {
				g_print ("module loaded: name is %s\n", priv->image_module->module_name);
				priv->context = (priv->image_module->begin_load) (gdk_pixbuf_loader_prepare, loader);
				retval = (priv->image_module->load_increment) (priv->context, priv->buf, 128);

				/* if we had more then 128 bytes total, we want to send the rest of the buffer */
				if (retval && (priv->buf_offset + count) >= 128) {
					retval = (priv->image_module->load_increment) (priv->context,
										       buf,
										       count + priv->buf_offset - 128);
				}
			}
		} else {
			priv->buf_offset += count;
		}
		return retval;
	}

	if (priv->image_module->load_increment)
		return (priv->image_module->load_increment) (priv->context,
							     buf,
							     count);
	return (FALSE);
}

/**
 * gdk_pixbuf_loader_get_pixbuf:
 * @loader: A loader.
 *
 * Gets the GdkPixbuf that the loader is currently loading.  If the loader
 * hasn't been enough data via gdk_pixbuf_loader_write, then NULL is returned.
 * Any application using this function should check for this value when it is
 * used.  The pixbuf returned will be the same in all future calls to the
 * loader, so simply calling a gdk_pixbuf_ref() should be sufficient to continue
 * using it.
 *
 * Return value: The GdkPixbuf that the loader is loading.
 **/
GdkPixbuf *
gdk_pixbuf_loader_get_pixbuf (GdkPixbufLoader *loader)
{
	GdkPixbufLoaderPrivate *priv;

	g_return_val_if_fail (loader != NULL, NULL);
	g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
	priv = loader->private;

	return priv->pixbuf;
}

/**
 * gdk_pixbuf_loader_close:
 * @loader: A loader.
 *
 * Tells the loader to stop accepting writes
 *
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
	if (priv->module == NULL) {
		priv->image_module = gdk_pixbuf_get_module (priv->buf, priv->buf_offset);
		if (priv->image_module &&
		    ((priv->image_module->begin_load == NULL) ||
		     (priv->image_module->begin_load == NULL) ||
		     (priv->image_module->begin_load == NULL) ||
		     (priv->image_module->begin_load == NULL))) {
			g_warning ("module %s does not support incremental loading.\n", priv->image_module->module_name);
		} else if (priv->image_module) {
			g_print ("module loaded: name is %s\n", priv->image_module->module_name);
			priv->context = (priv->image_module->begin_load) (gdk_pixbuf_loader_prepare, loader);
			(priv->image_module->load_increment) (priv->context, priv->buf, priv->buf_offset);
		}
	}

	if (priv->image_module && priv->image_module->stop_load)
		(priv->image_module->stop_load) (loader->context);

	priv->closed = TRUE;
}

