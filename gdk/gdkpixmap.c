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
#include "gdkpixmap.h"
#include "gdkinternals.h"
#include "gdkpixbuf.h"
#include "gdkscreen.h"
#include "gdkalias.h"

static GdkGC *gdk_pixmap_create_gc      (GdkDrawable     *drawable,
                                         GdkGCValues     *values,
                                         GdkGCValuesMask  mask);
static void   gdk_pixmap_draw_rectangle (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gboolean         filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void   gdk_pixmap_draw_arc       (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gboolean         filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 gint             angle1,
					 gint             angle2);
static void   gdk_pixmap_draw_polygon   (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gboolean         filled,
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

static void gdk_pixmap_draw_glyphs             (GdkDrawable      *drawable,
						GdkGC            *gc,
						PangoFont        *font,
						gint              x,
						gint              y,
						PangoGlyphString *glyphs);
static void gdk_pixmap_draw_glyphs_transformed (GdkDrawable      *drawable,
						GdkGC            *gc,
						PangoMatrix      *matrix,
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
static void   gdk_pixmap_draw_pixbuf    (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPixbuf       *pixbuf,
					 gint             src_x,
					 gint             src_y,
					 gint             dest_x,
					 gint             dest_y,
					 gint             width,
					 gint             height,
					 GdkRgbDither     dither,
					 gint             x_dither,
					 gint             y_dither);
static void  gdk_pixmap_draw_trapezoids (GdkDrawable     *drawable,
					 GdkGC	         *gc,
					 GdkTrapezoid    *trapezoids,
					 gint             n_trapezoids);

static void   gdk_pixmap_real_get_size  (GdkDrawable     *drawable,
                                         gint            *width,
                                         gint            *height);

static GdkImage* gdk_pixmap_copy_to_image (GdkDrawable *drawable,
					   GdkImage    *image,
					   gint         src_x,
					   gint         src_y,
					   gint         dest_x,
					   gint         dest_y,
					   gint         width,
					   gint         height);

static cairo_surface_t *gdk_pixmap_ref_cairo_surface (GdkDrawable *drawable);

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
  drawable_class->draw_glyphs_transformed = gdk_pixmap_draw_glyphs_transformed;
  drawable_class->draw_image = gdk_pixmap_draw_image;
  drawable_class->draw_pixbuf = gdk_pixmap_draw_pixbuf;
  drawable_class->draw_trapezoids = gdk_pixmap_draw_trapezoids;
  drawable_class->get_depth = gdk_pixmap_real_get_depth;
  drawable_class->get_screen = gdk_pixmap_real_get_screen;
  drawable_class->get_size = gdk_pixmap_real_get_size;
  drawable_class->set_colormap = gdk_pixmap_real_set_colormap;
  drawable_class->get_colormap = gdk_pixmap_real_get_colormap;
  drawable_class->get_visual = gdk_pixmap_real_get_visual;
  drawable_class->_copy_to_image = gdk_pixmap_copy_to_image;
  drawable_class->ref_cairo_surface = gdk_pixmap_ref_cairo_surface;
}

static void
gdk_pixmap_finalize (GObject *object)
{
  GdkPixmapObject *obj = (GdkPixmapObject *) object;

  g_object_unref (obj->impl);
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
			   gboolean     filled,
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
		     gboolean     filled,
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
			 gboolean     filled,
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
gdk_pixmap_draw_glyphs_transformed (GdkDrawable      *drawable,
				    GdkGC            *gc,
				    PangoMatrix      *matrix,
				    PangoFont        *font,
				    gint              x,
				    gint              y,
				    PangoGlyphString *glyphs)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_glyphs_transformed (private->impl, gc, matrix, font, x, y, glyphs);
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
gdk_pixmap_draw_pixbuf (GdkDrawable     *drawable,
			GdkGC           *gc,
			GdkPixbuf       *pixbuf,
			gint             src_x,
			gint             src_y,
			gint             dest_x,
			gint             dest_y,
			gint             width,
			gint             height,
			GdkRgbDither     dither,
			gint             x_dither,
			gint             y_dither)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_pixbuf (private->impl, gc, pixbuf,
		   src_x, src_y, dest_x, dest_y, width, height,
		   dither, x_dither, y_dither);
}

static void
gdk_pixmap_draw_trapezoids (GdkDrawable     *drawable,
			    GdkGC	    *gc,
			    GdkTrapezoid    *trapezoids,
			    gint             n_trapezoids)
{
  GdkPixmapObject *private = (GdkPixmapObject *)drawable;

  gdk_draw_trapezoids (private->impl, gc, trapezoids, n_trapezoids);
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

static GdkImage*
gdk_pixmap_copy_to_image (GdkDrawable     *drawable,
			  GdkImage        *image,
			  gint             src_x,
			  gint             src_y,
			  gint             dest_x,
			  gint             dest_y,
			  gint             width,
			  gint             height)
{
  g_return_val_if_fail (GDK_IS_PIXMAP (drawable), NULL);
  
  return gdk_drawable_copy_to_image (((GdkPixmapObject*)drawable)->impl,
				     image,
				     src_x, src_y, dest_x, dest_y,
				     width, height);
}

static cairo_surface_t *
gdk_pixmap_ref_cairo_surface (GdkDrawable *drawable)
{
  return _gdk_drawable_ref_cairo_surface (((GdkPixmapObject*)drawable)->impl);
}

static GdkBitmap *
make_solid_mask (GdkScreen *screen, gint width, gint height)
{
  GdkBitmap *bitmap;
  GdkGC *gc;
  GdkGCValues gc_values;
  
  bitmap = gdk_pixmap_new (gdk_screen_get_root_window (screen),
			   width, height, 1);

  gc_values.foreground.pixel = 1;
  gc = gdk_gc_new_with_values (bitmap, &gc_values, GDK_GC_FOREGROUND);
  
  gdk_draw_rectangle (bitmap, gc, TRUE, 0, 0, width, height);
  
  g_object_unref (gc);
  
  return bitmap;
}

#define PACKED_COLOR(c) ((((c)->red   & 0xff00)  << 8) |   \
			  ((c)->green & 0xff00)        |   \
			  ((c)->blue             >> 8))

static GdkPixmap *
gdk_pixmap_colormap_new_from_pixbuf (GdkColormap    *colormap,
				     GdkBitmap     **mask,
				     const GdkColor *transparent_color,
				     GdkPixbuf      *pixbuf)
{
  GdkPixmap *pixmap;
  GdkPixbuf *render_pixbuf;
  GdkGC *tmp_gc;
  GdkScreen *screen = gdk_colormap_get_screen (colormap);
  
  pixmap = gdk_pixmap_new (gdk_screen_get_root_window (screen),
			   gdk_pixbuf_get_width (pixbuf),
			   gdk_pixbuf_get_height (pixbuf),
			   gdk_colormap_get_visual (colormap)->depth);
  gdk_drawable_set_colormap (pixmap, colormap);
  
  if (transparent_color)
    {
      guint32 packed_color = PACKED_COLOR (transparent_color);
      render_pixbuf = gdk_pixbuf_composite_color_simple (pixbuf,
							 gdk_pixbuf_get_width (pixbuf),
							 gdk_pixbuf_get_height (pixbuf),
							 GDK_INTERP_NEAREST,
							 255, 16, packed_color, packed_color);
    }
  else
    render_pixbuf = pixbuf;

  tmp_gc = _gdk_drawable_get_scratch_gc (pixmap, FALSE);
  gdk_draw_pixbuf (pixmap, tmp_gc, render_pixbuf, 0, 0, 0, 0,
		   gdk_pixbuf_get_width (render_pixbuf),
		   gdk_pixbuf_get_height (render_pixbuf),
		   GDK_RGB_DITHER_NORMAL, 0, 0);

  if (render_pixbuf != pixbuf)
    g_object_unref (render_pixbuf);

  if (mask)
    gdk_pixbuf_render_pixmap_and_mask_for_colormap (pixbuf, colormap, NULL, mask, 128);

  if (mask && !*mask)
    *mask = make_solid_mask (screen,
			     gdk_pixbuf_get_width (pixbuf),
			     gdk_pixbuf_get_height (pixbuf));

  return pixmap;
}

GdkPixmap*
gdk_pixmap_colormap_create_from_xpm (GdkDrawable    *drawable,
				     GdkColormap    *colormap,
				     GdkBitmap     **mask,
				     const GdkColor *transparent_color,
				     const gchar    *filename)
{
  GdkPixbuf *pixbuf;
  GdkPixmap *pixmap;

  g_return_val_if_fail (drawable != NULL || colormap != NULL, NULL);
  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (colormap == NULL || GDK_IS_COLORMAP (colormap), NULL);

  if (colormap == NULL)
    colormap = gdk_drawable_get_colormap (drawable);
  
  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  if (!pixbuf)
    return NULL;

  pixmap = gdk_pixmap_colormap_new_from_pixbuf (colormap, mask, transparent_color, pixbuf);

  g_object_unref (pixbuf);
  
  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_xpm (GdkDrawable    *drawable,
			    GdkBitmap     **mask,
			    const GdkColor *transparent_color,
			    const gchar    *filename)
{
  return gdk_pixmap_colormap_create_from_xpm (drawable, NULL, mask,
					      transparent_color, filename);
}

GdkPixmap*
gdk_pixmap_colormap_create_from_xpm_d (GdkDrawable    *drawable,
				       GdkColormap    *colormap,
				       GdkBitmap     **mask,
				       const GdkColor *transparent_color,
				       gchar         **data)
{
  GdkPixbuf *pixbuf;
  GdkPixmap *pixmap;

  g_return_val_if_fail (drawable != NULL || colormap != NULL, NULL);
  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (colormap == NULL || GDK_IS_COLORMAP (colormap), NULL);

  if (colormap == NULL)
    colormap = gdk_drawable_get_colormap (drawable);
  
  pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)data);
  if (!pixbuf)
    return NULL;

  pixmap = gdk_pixmap_colormap_new_from_pixbuf (colormap, mask, transparent_color, pixbuf);

  g_object_unref (pixbuf);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_xpm_d (GdkDrawable    *drawable,
			      GdkBitmap     **mask,
			      const GdkColor *transparent_color,
			      gchar         **data)
{
  return gdk_pixmap_colormap_create_from_xpm_d (drawable, NULL, mask,
						transparent_color, data);
}

static GdkScreen*
gdk_pixmap_real_get_screen (GdkDrawable *drawable)
{
    return gdk_drawable_get_screen (GDK_PIXMAP_OBJECT (drawable)->impl);
}

#define __GDK_PIXMAP_C__
#include "gdkaliasdef.c"
