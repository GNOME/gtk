/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
#include <gtk/gtksignal.h>



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

static guint pixbuf_loader_signals[LAST_SIGNAL] = { 0 };



/* Internal data */

#define LOADER_HEADER_SIZE 128

typedef struct {
	GdkPixbuf *pixbuf;
	gboolean closed;
	gchar header_buf[LOADER_HEADER_SIZE];
	gint header_buf_offset;
	GdkPixbufModule *image_module;
	gpointer context;
} GdkPixbufLoaderPrivate;



/* our marshaller */
typedef void (* GtkSignal_NONE__INT_INT_INT_INT) (GtkObject *object,
						  gint arg1, gint arg2, gint arg3, gint arg4,
						  gpointer user_data);
static void
gtk_marshal_NONE__INT_INT_INT_INT (GtkObject *object, GtkSignalFunc func, gpointer func_data,
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



/**
 * gdk_pixbuf_loader_get_type:
 * @void: 
 * 
 * Registers the &GdkPixubfLoader class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GdkPixbufLoader class.
 **/
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
gdk_pixbuf_loader_class_init (GdkPixbufLoaderClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gtk_object_get_type ());

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
				GTK_TYPE_NONE, 4,
				GTK_TYPE_INT,
				GTK_TYPE_INT,
				GTK_TYPE_INT,
				GTK_TYPE_INT);

	pixbuf_loader_signals[CLOSED] =
		gtk_signal_new ("closed",
				GTK_RUN_LAST,
				parent_class->type,
				GTK_SIGNAL_OFFSET (GdkPixbufLoaderClass, closed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, pixbuf_loader_signals, LAST_SIGNAL);

	object_class->destroy = gdk_pixbuf_loader_destroy;
	object_class->finalize = gdk_pixbuf_loader_finalize;
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

	if (priv->pixbuf)
		gdk_pixbuf_unref (priv->pixbuf);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gdk_pixbuf_loader_finalize (GtkObject *object)
{
	GdkPixbufLoader *loader;
	GdkPixbufLoaderPrivate *priv = NULL;

	loader = GDK_PIXBUF_LOADER (object);
	priv = loader->private;

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gdk_pixbuf_loader_prepare (GdkPixbuf *pixbuf, gpointer loader)
{
	GdkPixbufLoaderPrivate *priv = NULL;

	priv = GDK_PIXBUF_LOADER (loader)->private;
	gdk_pixbuf_ref (pixbuf);
	g_assert (priv->pixbuf == NULL);

	priv->pixbuf = pixbuf;
	gtk_signal_emit (GTK_OBJECT (loader), pixbuf_loader_signals[AREA_PREPARED]);
}



/**
 * gdk_pixbuf_loader_new:
 * @void: 
 * 
 * Creates a new pixbuf loader object.
 * 
 * Return value: A newly-created pixbuf loader.
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new (void)
{
	return gtk_type_new (gdk_pixbuf_loader_get_type ());
}

static int
gdk_pixbuf_loader_load_module(GdkPixbufLoader *loader)
{
	GdkPixbufLoaderPrivate *priv = loader->private;

	priv->image_module = gdk_pixbuf_get_module (priv->header_buf, priv->header_buf_offset);

	if (priv->image_module == NULL)
		return 0;

	if (priv->image_module->module == NULL)
		gdk_pixbuf_load_module (priv->image_module);

	if (priv->image_module->module == NULL)
		return 0;

	if ((priv->image_module->begin_load == NULL) ||
	    (priv->image_module->stop_load == NULL) ||
	    (priv->image_module->load_increment == NULL)) {
		g_warning ("module %s does not support incremental loading.\n",
			   priv->image_module->module_name);
		return 0;
	}

	priv->context = (*priv->image_module->begin_load) (gdk_pixbuf_loader_prepare, loader);

	if (priv->context == NULL) {
		g_warning("Failed to begin progressive load");
		return 0;
	}

	if( (* priv->image_module->load_increment) (priv->context, priv->header_buf, priv->header_buf_offset) )
		return priv->header_buf_offset;
 
	return 0;
}

static int
gdk_pixbuf_loader_eat_header_write (GdkPixbufLoader *loader, guchar *buf, size_t count)
{
	int nbytes;
	GdkPixbufLoaderPrivate *priv = loader->private;

	nbytes = MIN(LOADER_HEADER_SIZE - priv->header_buf_offset, count);
	memcpy (priv->header_buf + priv->header_buf_offset, buf, nbytes);
	    
	priv->header_buf_offset += nbytes;
	    
	if(priv->header_buf_offset >= LOADER_HEADER_SIZE) {
		return gdk_pixbuf_loader_load_module(loader);
	} else
		return nbytes;
}

/**
 * gdk_pixbuf_loader_write:
 * @loader: A pixbuf loader.
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
gdk_pixbuf_loader_write (GdkPixbufLoader *loader, guchar *buf, size_t count)
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
		int eaten;

		eaten = gdk_pixbuf_loader_eat_header_write(loader, buf, count);
		if (eaten <= 0)
			return FALSE;

		count -= eaten;
		buf += eaten;
	}

	if (count > 0 && priv->image_module->load_increment)
		return (* priv->image_module->load_increment) (priv->context, buf, count);

	return TRUE;
}

/**
 * gdk_pixbuf_loader_get_pixbuf:
 * @loader: A pixbuf loader.
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
 * @loader: A pixbuf loader.
 *
 * Tells the loader to stop accepting writes.
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
		gdk_pixbuf_loader_load_module (loader);

	if (priv->image_module && priv->image_module->stop_load)
		(* priv->image_module->stop_load) (priv->context);

	priv->closed = TRUE;
}
