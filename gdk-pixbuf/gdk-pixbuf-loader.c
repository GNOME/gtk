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


static GtkObjectClass *parent_class; 

static void gdk_pixbuf_loader_class_init    (GdkPixbufLoaderClass   *klass);
static void gdk_pixbuf_loader_init          (GdkPixbufLoader        *loader);
static void gdk_pixbuf_loader_destroy       (GdkPixbufLoader        *loader);
static void gdk_pixbuf_loader_finalize      (GdkPixbufLoader        *loader);

/* Internal data */
typedef struct _GdkPixbufLoaderPrivate GdkPixbufLoaderPrivate;
struct _GdkPixbufLoaderPrivate
{
	GdkPixbuf *pixbuf;
	gboolean closed;
};

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

	parent_class->destroy = gdk_pixbuf_loader_destroy;
	parent_class->finalize = gdk_pixbuf_loader_finalize;
}

static void
gdk_pixbuf_loader_init (GdkPixbufLoader *loader)
{
	GdkPixbuf *pixbuf;
	loader->private = g_new (GdkPixbufLoaderPrivate, 1);

	loader->pixbuf = NULL;
	loader->closed = FALSE;
}

static void
gdk_pixbuf_loader_destroy (GdkPixbufLoader *loader)
{
	GdkPixbufLoaderPrivate *priv;

	priv = loader->private;
	gdk_pixbuf_unref (priv->pixbuf);
}

static void
gdk_pixbuf_loader_finalize (GdkPixbufLoader *loader)
{
	GdkPixbufLoaderPrivate *priv;

	priv = loader->private;
	g_free (priv);
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
gdk_pixbuf_loader_write (GdkPixbufLoader *loader, gchar *buf, size_t count)
{
	GdkPixbufLoaderPrivate *priv;

	g_return_val_if_fail (loader != NULL, FALSE);
	g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), FALSE);

	priv = loader->private;

	/* we expect it's not to be closed */
	g_return_val_if_fail (priv->closed == FALSE, FALSE);

	return TRUE;
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
	priv->closed = TRUE;
}

