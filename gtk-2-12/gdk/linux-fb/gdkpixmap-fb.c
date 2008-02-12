/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Needed for SEEK_END in SunOS */
#include <unistd.h>

#include "gdkpixmap.h"
#include "gdkfb.h"
#include "gdkprivate-fb.h"

typedef struct
{
  gchar *color_string;
  GdkColor color;
  gint transparent;
} _GdkPixmapColor;

typedef struct
{
  guint ncolors;
  GdkColormap *colormap;
  gulong pixels[1];
} _GdkPixmapInfo;

static gpointer parent_class = NULL;

static void
gdk_pixmap_impl_fb_init (GdkPixmapFBData *impl)
{
  GdkDrawableFBData *private = (GdkDrawableFBData *)impl;

  private->window_type = GDK_DRAWABLE_PIXMAP;
  private->colormap = gdk_colormap_ref (gdk_colormap_get_system());
  private->mem = NULL;
  private->width = 1;
  private->height = 1;
}

static void
gdk_pixmap_impl_fb_finalize (GObject *object)
{
  g_free (GDK_DRAWABLE_FBDATA (object)->mem);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_pixmap_impl_fb_class_init (GdkPixmapFBClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  /*  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass); */
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_impl_fb_finalize;
}

GType
_gdk_pixmap_impl_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkPixmapFBClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_pixmap_impl_fb_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkPixmapFBData),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_pixmap_impl_fb_init
      };
      
      object_type = g_type_register_static (gdk_drawable_impl_fb_get_type(),
                                            "GdkPixmapFB",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GdkPixmap*
gdk_pixmap_new (GdkDrawable *window,
		gint         width,
		gint         height,
		gint         depth)
{
  GdkPixmap *pixmap;
  GdkDrawableFBData *private;

  g_return_val_if_fail (window == NULL || GDK_IS_DRAWABLE (window), NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  
  if (!window)
    window = _gdk_parent_root;

  if (depth == -1)
    depth = gdk_drawable_get_visual (window)->depth;

  pixmap = (GdkPixmap *)g_object_new (gdk_pixmap_get_type (), NULL);
  private = GDK_DRAWABLE_IMPL_FBDATA (pixmap);
  private->rowstride = (width * depth + 7) / 8; /* Round up to nearest whole byte */
  private->mem = g_malloc (private->rowstride * height);
  private->abs_x = 0;
  private->abs_y = 0;
  private->llim_x = 0;
  private->llim_y = 0;
  private->lim_x = width;
  private->lim_y = height;
  private->width = width;
  private->height = height;
  private->depth = ((GdkPixmapObject *)pixmap)->depth = depth;

  return pixmap;
}

GdkPixmap *
gdk_bitmap_create_from_data (GdkDrawable *window,
			     const gchar *data,
			     gint         width,
			     gint         height)
{
  GdkPixmap *pixmap;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  g_return_val_if_fail (window == NULL || GDK_IS_DRAWABLE (window), NULL);

  if (!window)
    window = _gdk_parent_root;

  pixmap = gdk_pixmap_new (window, width, height, 1);

  memcpy (GDK_DRAWABLE_IMPL_FBDATA (pixmap)->mem, data, ((width + 7) / 8) * height);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_data (GdkDrawable    *window,
			     const gchar    *data,
			     gint            width,
			     gint            height,
			     gint            depth,
			     const GdkColor *fg,
			     const GdkColor *bg)
{
  GdkPixmap *pixmap;

  g_return_val_if_fail (window == NULL || GDK_IS_DRAWABLE (window), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!window)
    window = _gdk_parent_root;

  if (depth == -1)
    depth = gdk_drawable_get_visual (window)->depth;

  pixmap = gdk_pixmap_new (window, width, height, depth);

  memcpy (GDK_DRAWABLE_IMPL_FBDATA (pixmap)->mem, data, height * GDK_DRAWABLE_IMPL_FBDATA (pixmap)->rowstride);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
  return (GdkPixmap*) gdk_drawable_ref ((GdkDrawable *)anid);
}

GdkPixmap*
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  return (GdkPixmap*) (anid);
}

