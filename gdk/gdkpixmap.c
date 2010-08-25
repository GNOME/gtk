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

#include "config.h"
#include "gdkpixmap.h"
#include "gdkinternals.h"
#include "gdkpixbuf.h"
#include "gdkscreen.h"


static void   gdk_pixmap_real_get_size  (GdkDrawable     *drawable,
                                         gint            *width,
                                         gint            *height);

static cairo_surface_t *gdk_pixmap_ref_cairo_surface (GdkDrawable *drawable);
static cairo_surface_t *gdk_pixmap_create_cairo_surface (GdkDrawable *drawable,
							 int width,
							 int height);

static GdkVisual*   gdk_pixmap_real_get_visual   (GdkDrawable *drawable);
static gint         gdk_pixmap_real_get_depth    (GdkDrawable *drawable);
static void         gdk_pixmap_real_set_colormap (GdkDrawable *drawable,
                                                  GdkColormap *cmap);
static GdkColormap* gdk_pixmap_real_get_colormap (GdkDrawable *drawable);
static GdkScreen*   gdk_pixmap_real_get_screen   (GdkDrawable *drawable);

static void gdk_pixmap_init       (GdkPixmapObject      *pixmap);
static void gdk_pixmap_class_init (GdkPixmapObjectClass *klass);
static void gdk_pixmap_finalize   (GObject              *object);

static gpointer parent_class = NULL;

GType
gdk_pixmap_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    object_type = g_type_register_static_simple (GDK_TYPE_DRAWABLE,
						 "GdkPixmap",
						 sizeof (GdkPixmapObjectClass),
						 (GClassInitFunc) gdk_pixmap_class_init,
						 sizeof (GdkPixmapObject),
						 (GInstanceInitFunc) gdk_pixmap_init,
						 0);
  
  return object_type;
}

static void
gdk_pixmap_init (GdkPixmapObject *pixmap)
{
  /* 0-initialization is good for all other fields. */
  pixmap->impl = g_object_new (_gdk_pixmap_impl_get_type (), NULL);
}

static void
gdk_pixmap_class_init (GdkPixmapObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_finalize;

  drawable_class->get_depth = gdk_pixmap_real_get_depth;
  drawable_class->get_screen = gdk_pixmap_real_get_screen;
  drawable_class->get_size = gdk_pixmap_real_get_size;
  drawable_class->set_colormap = gdk_pixmap_real_set_colormap;
  drawable_class->get_colormap = gdk_pixmap_real_get_colormap;
  drawable_class->get_visual = gdk_pixmap_real_get_visual;
  drawable_class->ref_cairo_surface = gdk_pixmap_ref_cairo_surface;
  drawable_class->create_cairo_surface = gdk_pixmap_create_cairo_surface;
}

static void
gdk_pixmap_finalize (GObject *object)
{
  GdkPixmapObject *obj = (GdkPixmapObject *) object;

  g_object_unref (obj->impl);
  obj->impl = NULL;
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GdkPixmap *
gdk_pixmap_new (GdkDrawable *drawable,
                gint         width,
                gint         height,
                gint         depth)
{
  GdkDrawable *source_drawable;

  if (drawable)
    source_drawable = _gdk_drawable_get_source_drawable (drawable);
  else
    source_drawable = NULL;
  return _gdk_pixmap_new (source_drawable, width, height, depth);
}

static void
gdk_pixmap_real_get_size (GdkDrawable *drawable,
                          gint *width,
                          gint *height)
{
  g_return_if_fail (GDK_IS_PIXMAP (drawable));

  gdk_drawable_get_size (GDK_DRAWABLE (((GdkPixmapObject*)drawable)->impl),
                         width, height);
}

static GdkVisual*
gdk_pixmap_real_get_visual (GdkDrawable *drawable)
{
  GdkColormap *colormap;

  g_return_val_if_fail (GDK_IS_PIXMAP (drawable), NULL);

  colormap = gdk_drawable_get_colormap (drawable);
  return colormap ? gdk_colormap_get_visual (colormap) : NULL;
}

static gint
gdk_pixmap_real_get_depth (GdkDrawable *drawable)
{
  gint depth;
  
  g_return_val_if_fail (GDK_IS_PIXMAP (drawable), 0);

  depth = GDK_PIXMAP_OBJECT (drawable)->depth;

  return depth;
}

static void
gdk_pixmap_real_set_colormap (GdkDrawable *drawable,
                              GdkColormap *cmap)
{
  g_return_if_fail (GDK_IS_PIXMAP (drawable));  
  
  gdk_drawable_set_colormap (((GdkPixmapObject*)drawable)->impl, cmap);
}

static GdkColormap*
gdk_pixmap_real_get_colormap (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_PIXMAP (drawable), NULL);
  
  return gdk_drawable_get_colormap (((GdkPixmapObject*)drawable)->impl);
}

static cairo_surface_t *
gdk_pixmap_ref_cairo_surface (GdkDrawable *drawable)
{
  return _gdk_drawable_ref_cairo_surface (((GdkPixmapObject*)drawable)->impl);
}

static cairo_surface_t *
gdk_pixmap_create_cairo_surface (GdkDrawable *drawable,
				 int width,
				 int height)
{
  return _gdk_drawable_create_cairo_surface (GDK_PIXMAP_OBJECT(drawable)->impl,
					     width, height);
}

static GdkScreen*
gdk_pixmap_real_get_screen (GdkDrawable *drawable)
{
    return gdk_drawable_get_screen (GDK_PIXMAP_OBJECT (drawable)->impl);
}
