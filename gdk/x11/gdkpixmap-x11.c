/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Needed for SEEK_END in SunOS */
#include <unistd.h>
#include <X11/Xlib.h>

#include <gdk/gdkpixmap.h>
#include "gdkpixmap-x11.h"
#include "gdkprivate-x11.h"

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

static void gdk_pixmap_impl_x11_get_size   (GdkDrawable        *drawable,
                                        gint               *width,
                                        gint               *height);

static void gdk_pixmap_impl_x11_init       (GdkPixmapImplX11      *pixmap);
static void gdk_pixmap_impl_x11_class_init (GdkPixmapImplX11Class *klass);
static void gdk_pixmap_impl_x11_finalize   (GObject            *object);

static gpointer parent_class = NULL;

GType
gdk_pixmap_impl_x11_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkPixmapImplX11Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_pixmap_impl_x11_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkPixmapImplX11),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_pixmap_impl_x11_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_X11,
                                            "GdkPixmapImplX11",
                                            &object_info);
    }
  
  return object_type;
}


GType
_gdk_pixmap_impl_get_type (void)
{
  return gdk_pixmap_impl_x11_get_type ();
}

static void
gdk_pixmap_impl_x11_init (GdkPixmapImplX11 *impl)
{
  impl->width = 1;
  impl->height = 1;
}

static void
gdk_pixmap_impl_x11_class_init (GdkPixmapImplX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_impl_x11_finalize;

  drawable_class->get_size = gdk_pixmap_impl_x11_get_size;
}

static void
gdk_pixmap_impl_x11_finalize (GObject *object)
{
  GdkPixmapImplX11 *impl = GDK_PIXMAP_IMPL_X11 (object);
  GdkPixmap *wrapper = GDK_PIXMAP (GDK_DRAWABLE_IMPL_X11 (impl)->wrapper);
  
  XFreePixmap (GDK_PIXMAP_XDISPLAY (wrapper), GDK_PIXMAP_XID (wrapper));
  gdk_xid_table_remove (GDK_PIXMAP_XID (wrapper));
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_pixmap_impl_x11_get_size   (GdkDrawable *drawable,
                                gint        *width,
                                gint        *height)
{
  if (width)
    *width = GDK_PIXMAP_IMPL_X11 (drawable)->width;
  if (height)
    *height = GDK_PIXMAP_IMPL_X11 (drawable)->height;
}

GdkPixmap*
gdk_pixmap_new (GdkWindow *window,
		gint       width,
		gint       height,
		gint       depth)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  
  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  
  if (!window)
    window = gdk_parent_root;

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  if (depth == -1)
    depth = gdk_drawable_get_depth (GDK_DRAWABLE (window));

  pixmap = GDK_PIXMAP (g_type_create_instance (gdk_pixmap_get_type ()));
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  draw_impl->xdisplay = GDK_WINDOW_XDISPLAY (window);
  draw_impl->xid = XCreatePixmap (GDK_PIXMAP_XDISPLAY (pixmap),
                                  GDK_WINDOW_XID (window),
                                  width, height, depth);
  
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  gdk_xid_table_insert (&GDK_PIXMAP_XID (pixmap), pixmap);

  return pixmap;
}

GdkPixmap *
gdk_bitmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);

  if (!window)
    window = gdk_parent_root;

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  pixmap = GDK_PIXMAP (g_type_create_instance (gdk_pixmap_get_type ()));
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);

  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = 1;

  draw_impl->xdisplay = GDK_WINDOW_XDISPLAY (window);
  draw_impl->xid = XCreateBitmapFromData (GDK_WINDOW_XDISPLAY (window),
                                          GDK_WINDOW_XID (window),
                                          (char *)data, width, height);

  gdk_xid_table_insert (&GDK_PIXMAP_XID (pixmap), pixmap);
  
  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height,
			     gint         depth,
			     GdkColor    *fg,
			     GdkColor    *bg)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!window)
    window = gdk_parent_root;

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  if (depth == -1)
    depth = gdk_drawable_get_visual (window)->depth;

  pixmap = GDK_PIXMAP (g_type_create_instance (gdk_pixmap_get_type ()));
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  draw_impl->xdisplay = GDK_DRAWABLE_XDISPLAY (window);
  draw_impl->xid = XCreatePixmapFromBitmapData (GDK_WINDOW_XDISPLAY (window),
                                                GDK_WINDOW_XID (window),
                                                (char *)data, width, height,
                                                fg->pixel, bg->pixel, depth);

  gdk_xid_table_insert (&GDK_PIXMAP_XID (pixmap), pixmap);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  Pixmap xpixmap;
  Window root_return;
  unsigned int x_ret, y_ret, w_ret, h_ret, bw_ret, depth_ret;

  /* check to make sure we were passed something at
     least a little sane */
  g_return_val_if_fail((anid != 0), NULL);
  
  /* set the pixmap to the passed in value */
  xpixmap = anid;

  /* get information about the Pixmap to fill in the structure for
     the gdk window */
  if (!XGetGeometry(GDK_DISPLAY(),
		    xpixmap, &root_return,
		    &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
      return NULL;

  pixmap = GDK_PIXMAP (g_type_create_instance (gdk_pixmap_get_type ()));
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  

  draw_impl->xdisplay = GDK_DISPLAY ();
  draw_impl->xid = xpixmap;

  pix_impl->width = w_ret;
  pix_impl->height = h_ret;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth_ret;
  
  gdk_xid_table_insert(&GDK_PIXMAP_XID (pixmap), pixmap);

  return pixmap;
}
