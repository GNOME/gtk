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

#include "gdkpixmap.h"
#include "gdkinternals.h"

static GdkGC *gdk_pixmap_create_gc      (GdkDrawable     *drawable,
                                         GdkGCValues     *values,
                                         GdkGCValuesMask  mask);
static void   gdk_pixmap_draw_rectangle (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gint             filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void   gdk_pixmap_draw_arc       (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gint             filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 gint             angle1,
					 gint             angle2);
static void   gdk_pixmap_draw_polygon   (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gint             filled,
					 GdkPoint        *points,
					 gint             npoints);
static void   gdk_pixmap_draw_text      (GdkDrawable     *drawable,
					 GdkFont         *font,
					 GdkGC           *gc,
					 gint             x,
					 gint             y,
					 const gchar     *text,
					 gint             text_length);
static void   gdk_pixmap_draw_text_wc   (GdkDrawable     *drawable,
					 GdkFont         *font,
					 GdkGC           *gc,
					 gint             x,
					 gint             y,
					 const GdkWChar  *text,
					 gint             text_length);
static void   gdk_pixmap_draw_drawable  (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPixmap       *src,
					 gint             xsrc,
					 gint             ysrc,
					 gint             xdest,
					 gint             ydest,
					 gint             width,
					 gint             height);
static void   gdk_pixmap_draw_points    (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPoint        *points,
					 gint             npoints);
static void   gdk_pixmap_draw_segments  (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkSegment      *segs,
					 gint             nsegs);
static void   gdk_pixmap_draw_lines     (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPoint        *points,
					 gint             npoints);
static void   gdk_pixmap_draw_glyphs    (GdkDrawable      *drawable,
                                         GdkGC            *gc,
                                         PangoFont        *font,
                                         gint              x,
                                         gint              y,
                                         PangoGlyphString *glyphs);
static void   gdk_pixmap_draw_image     (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         GdkImage        *image,
                                         gint             xsrc,
                                         gint             ysrc,
                                         gint             xdest,
                                         gint             ydest,
                                         gint             width,
                                         gint             height);

static void   gdk_pixmap_real_get_size  (GdkDrawable     *drawable,
                                         gint            *width,
                                         gint            *height);

static GdkVisual*   gdk_pixmap_real_get_visual   (GdkDrawable *drawable);
static gint         gdk_pixmap_real_get_depth    (GdkDrawable *drawable);
static void         gdk_pixmap_real_set_colormap (GdkDrawable *drawable,
                                                  GdkColormap *cmap);
static GdkColormap* gdk_pixmap_real_get_colormap (GdkDrawable *drawable);

static void gdk_pixmap_init       (GdkPixmapObject      *pixmap);
static void gdk_pixmap_class_init (GdkPixmapObjectClass *klass);
static void gdk_pixmap_finalize   (GObject              *object);

static gpointer parent_class = NULL;

GType
gdk_pixmap_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkPixmapObjectClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_pixmap_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkPixmapObject),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_pixmap_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkPixmap",
                                            &object_info);
    }
  
  return object_type;
}

static void
gdk_pixmap_init (GdkPixmapObject *pixmap)
{
  /* 0-initialization is good for all other fields. */
  pixmap->impl =
    GDK_DRAWABLE (g_type_create_instance (_gdk_pixmap_impl_get_type ()));
}

static void
gdk_pixmap_class_init (GdkPixmapObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_finalize;

  drawable_class->create_gc = gdk_pixmap_create_gc;
  drawable_class->draw_rectangle = gdk_pixmap_draw_rectangle;
  drawable_class->draw_arc = gdk_pixmap_draw_arc;
  drawable_class->draw_polygon = gdk_pixmap_draw_polygon;
  drawable_class->draw_text = gdk_pixmap_draw_text;
  drawable_class->draw_text_wc = gdk_pixmap_draw_text_wc;
  drawable_class->draw_drawable = gdk_pixmap_draw_drawable;
  drawable_class->draw_points = gdk_pixmap_draw_points;
  drawable_class->draw_segments = gdk_pixmap_draw_segments;
  drawable_class->draw_lines = gdk_pixmap_draw_lines;
  drawable_class->draw_glyphs = gdk_pixmap_draw_glyphs;
  drawable_class->draw_image = gdk_pixmap_draw_image;
  drawable_class->get_depth = gdk_pixmap_real_get_depth;
  drawable_class->get_size = gdk_pixmap_real_get_size;
  drawable_class->set_colormap = gdk_pixmap_real_set_colormap;
  drawable_class->get_colormap = gdk_pixmap_real_get_colormap;
  drawable_class->get_visual = gdk_pixmap_real_get_visual;
}

static void
gdk_pixmap_finalize (GObject *object)
{
  GdkPixmapObject *obj = (GdkPixmapObject *) object;

  g_object_unref (G_OBJECT (obj->impl));
  obj->impl = NULL;
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GdkGC *
gdk_pixmap_create_gc (GdkDrawable     *drawable,
                      GdkGCValues     *values,
                      GdkGCValuesMask  mask)
{
  return gdk_gc_new_with_values (((GdkPixmapObject *) drawable)->impl,
                                 values, mask);
}

static void
gdk_pixmap_draw_rectangle (GdkDrawable *drawable,
			   GdkGC       *gc,
			   gint         filled,
			   gint         x,
			   gint         y,
			   gint         width,
			   gint         height)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_rectangle (private->impl, gc, filled,
                      x, y, width, height);
}

static void
gdk_pixmap_draw_arc (GdkDrawable *drawable,
		     GdkGC       *gc,
		     gint         filled,
		     gint         x,
		     gint         y,
		     gint         width,
		     gint         height,
		     gint         angle1,
		     gint         angle2)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_arc (private->impl, gc, filled,
                x, y,
                width, height, angle1, angle2);
}

static void
gdk_pixmap_draw_polygon (GdkDrawable *drawable,
			 GdkGC       *gc,
			 gint         filled,
			 GdkPoint    *points,
			 gint         npoints)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_polygon (private->impl, gc, filled, points, npoints);
}

static void
gdk_pixmap_draw_text (GdkDrawable *drawable,
		      GdkFont     *font,
		      GdkGC       *gc,
		      gint         x,
		      gint         y,
		      const gchar *text,
		      gint         text_length)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_text (private->impl, font, gc,
                 x, y, text, text_length);
}

static void
gdk_pixmap_draw_text_wc (GdkDrawable    *drawable,
			 GdkFont        *font,
			 GdkGC          *gc,
			 gint            x,
			 gint            y,
			 const GdkWChar *text,
			 gint            text_length)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_text_wc (private->impl, font, gc,
                    x, y, text, text_length);
}

static void
gdk_pixmap_draw_drawable (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkPixmap   *src,
			  gint         xsrc,
			  gint         ysrc,
			  gint         xdest,
			  gint         ydest,
			  gint         width,
			  gint         height)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_drawable (private->impl, gc, src, xsrc, ysrc,
                     xdest, ydest,
                     width, height);
}

static void
gdk_pixmap_draw_points (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPoint    *points,
			gint         npoints)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_points (private->impl, gc, points, npoints);
}

static void
gdk_pixmap_draw_segments (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkSegment  *segs,
			  gint         nsegs)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_segments (private->impl, gc, segs, nsegs);
}

static void
gdk_pixmap_draw_lines (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_lines (private->impl, gc, points, npoints);
}

static void
gdk_pixmap_draw_glyphs (GdkDrawable      *drawable,
                        GdkGC            *gc,
                        PangoFont        *font,
                        gint              x,
                        gint              y,
                        PangoGlyphString *glyphs)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_glyphs (private->impl, gc, font, x, y, glyphs);
}

static void
gdk_pixmap_draw_image (GdkDrawable     *drawable,
                       GdkGC           *gc,
                       GdkImage        *image,
                       gint             xsrc,
                       gint             ysrc,
                       gint             xdest,
                       gint             ydest,
                       gint             width,
                       gint             height)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_image (private->impl, gc, image, xsrc, ysrc, xdest, ydest,
                  width, height);
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
