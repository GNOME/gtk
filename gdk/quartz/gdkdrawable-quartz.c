/* gdkdrawable-quartz.c
 *
 * Copyright (C) 2005, 2006 Imendio AB
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

#include <config.h>

#include <cairo-quartz.h>
#include "gdkprivate-quartz.h"

static gpointer parent_class;

typedef struct {
  GdkDrawable *drawable;
  CGContextRef context;
} SurfaceInfo;

static cairo_user_data_key_t surface_info_key;

static void
surface_info_destroy (void *data)
{
  SurfaceInfo *info = data;

  gdk_quartz_drawable_release_context (info->drawable, info->context);

  g_free (info);
}

static cairo_surface_t *
gdk_quartz_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);
  CGContextRef context;
  int width, height;
  cairo_surface_t *surface;
  SurfaceInfo *info;

  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  context = gdk_quartz_drawable_get_context (drawable, TRUE);
  if (!context)
    return NULL;

  gdk_drawable_get_size (drawable, &width, &height);

  surface = cairo_quartz_surface_create (context, width, height, TRUE);

  info = g_new (SurfaceInfo, 1);
  info->drawable = drawable;
  info->context = context;

  cairo_surface_set_user_data (surface, &surface_info_key,
			       info, surface_info_destroy);
  return surface;
}

static void
gdk_quartz_set_colormap (GdkDrawable *drawable,
			 GdkColormap *colormap)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);

  if (impl->colormap == colormap)
    return;
  
  if (impl->colormap)
    g_object_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    g_object_ref (impl->colormap);
}

static GdkColormap*
gdk_quartz_get_colormap (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_IMPL_QUARTZ (drawable)->colormap;
}

static GdkScreen*
gdk_quartz_get_screen (GdkDrawable *drawable)
{
  return _gdk_screen;
}

static GdkVisual*
gdk_quartz_get_visual (GdkDrawable *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper);
}

static int
gdk_quartz_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper);
}

static void
gdk_quartz_draw_rectangle (GdkDrawable *drawable,
			   GdkGC       *gc,
			   gboolean     filled,
			   gint         x,
			   gint         y,
			   gint         width,
			   gint         height)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);

  if (!context)
    return;

  gdk_quartz_update_context_from_gc (context, gc);

  if (filled)
    {
      CGRect rect = CGRectMake (x, y, width, height);

      gdk_quartz_set_context_fill_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						    _gdk_gc_get_fg_pixel (gc));
      CGContextFillRect (context, rect);
    }
  else
    {
      CGRect rect = CGRectMake (x + 0.5, y + 0.5, width, height);

      gdk_quartz_set_context_stroke_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						      _gdk_gc_get_fg_pixel (gc));
      CGContextStrokeRect (context, rect);
    }

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_arc (GdkDrawable *drawable,
		     GdkGC       *gc,
		     gboolean     filled,
		     gint         x,
		     gint         y,
		     gint         width,
		     gint         height,
		     gint         angle1,
		     gint         angle2)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  float start_angle, end_angle;

  if (!context)
    return;

  gdk_quartz_update_context_from_gc (context, gc);

  CGContextSaveGState (context);

  start_angle = (2 - (angle1 / (180.0 * 64.0))) * G_PI;
  end_angle = start_angle - (angle2 / (180.0 * 64.0)) * G_PI;

  if (filled)
    {
      gdk_quartz_set_context_fill_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
                                                    _gdk_gc_get_fg_pixel (gc));

      CGContextTranslateCTM (context,
                             x + width / 2.0,
                             y + height / 2.0);
      CGContextScaleCTM (context, 1.0, (double)height / (double)width);

      CGContextMoveToPoint (context, 0, 0);
      CGContextAddArc (context, 0, 0, width / 2.0,
		       start_angle, end_angle,
		       TRUE);
      CGContextClosePath (context);
      CGContextFillPath (context);
    }
  else
    {
      gdk_quartz_set_context_stroke_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						      _gdk_gc_get_fg_pixel (gc));

      CGContextTranslateCTM (context,
                             x + width / 2.0 + 0.5,
                             y + height / 2.0 + 0.5);
      CGContextScaleCTM (context, 1.0, (double)height / (double)width);

      CGContextAddArc (context, 0, 0, width / 2.0,
		       start_angle, end_angle,
		       TRUE);
      CGContextStrokePath (context);
    }

  CGContextRestoreGState (context);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_polygon (GdkDrawable *drawable,
			 GdkGC       *gc,
			 gboolean     filled,
			 GdkPoint    *points,
			 gint         npoints)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  int i;

  if (!context)
    return;

  gdk_quartz_update_context_from_gc (context, gc);

  if (filled)
    {
      gdk_quartz_set_context_fill_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						    _gdk_gc_get_fg_pixel (gc));

      CGContextMoveToPoint (context, points[0].x, points[0].y);
      for (i = 1; i < npoints; i++)
	CGContextAddLineToPoint (context, points[i].x, points[i].y);

      CGContextClosePath (context);
      CGContextFillPath (context);
    }
  else
    {
      gdk_quartz_set_context_stroke_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						      _gdk_gc_get_fg_pixel (gc));

      CGContextMoveToPoint (context, points[0].x + 0.5, points[0].y + 0.5);
      for (i = 1; i < npoints; i++)
	CGContextAddLineToPoint (context, points[i].x + 0.5, points[i].y + 0.5);

      CGContextClosePath (context);
      CGContextStrokePath (context);
    }

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_text (GdkDrawable *drawable,
		      GdkFont     *font,
		      GdkGC       *gc,
		      gint         x,
		      gint         y,
		      const gchar *text,
		      gint         text_length)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_draw_text_wc (GdkDrawable    *drawable,
			 GdkFont	*font,
			 GdkGC	        *gc,
			 gint	         x,
			 gint	         y,
			 const GdkWChar *text,
			 gint	         text_length)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_draw_drawable (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkPixmap   *src,
			  gint         xsrc,
			  gint         ysrc,
			  gint         xdest,
			  gint         ydest,
			  gint         width,
			  gint         height)
{
  int src_depth = gdk_drawable_get_depth (src);
  int dest_depth = gdk_drawable_get_depth (drawable);
  GdkDrawableImplQuartz *src_impl;

  if (GDK_IS_DRAWABLE_IMPL_QUARTZ (src))
    src_impl = GDK_DRAWABLE_IMPL_QUARTZ (src);
  else if (GDK_IS_PIXMAP (src))
    src_impl = GDK_DRAWABLE_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (src)->impl);
  else if (GDK_IS_WINDOW (src))
    {
      src_impl = GDK_DRAWABLE_IMPL_QUARTZ (GDK_WINDOW_OBJECT (src)->impl);
      /* FIXME: Implement drawing a window. */
      return;
    }
  else
    g_assert_not_reached ();
  
  if (src_depth == 1)
    {
      /* FIXME: src depth 1 is not supported yet */
    }
  else if (dest_depth != 0 && src_depth == dest_depth)
    {
      CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);

      if (!context)
	return;

      gdk_quartz_update_context_from_gc (context, gc);

      CGContextSetBlendMode (context, kCGBlendModeNormal);

      CGContextClipToRect (context, CGRectMake (xdest, ydest, width, height));
      CGContextTranslateCTM (context, xdest - xsrc, ydest - ysrc);
      CGContextDrawImage (context, 
			  CGRectMake(0, 0, 
				     GDK_PIXMAP_IMPL_QUARTZ (src_impl)->width, 
				     GDK_PIXMAP_IMPL_QUARTZ (src_impl)->height), 
			  GDK_PIXMAP_IMPL_QUARTZ (src_impl)->image);

      gdk_quartz_drawable_release_context (drawable, context);
    }
  else
    g_warning ("Attempt to draw a drawable with depth %d to a drawable with depth %d",
	       src_depth, dest_depth);
}

static void
gdk_quartz_draw_points (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPoint    *points,
			gint         npoints)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  int i;

  if (!context)
    return;

  gdk_quartz_update_context_from_gc (context, gc);
  gdk_quartz_set_context_fill_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						_gdk_gc_get_fg_pixel (gc));

  /* Just draw 1x1 rectangles */
  for (i = 0; i < npoints; i++) 
    {
      CGRect rect = CGRectMake (points[i].x, points[i].y, 1, 1);
      CGContextFillRect (context, rect);
    }

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_segments (GdkDrawable    *drawable,
			  GdkGC          *gc,
			  GdkSegment     *segs,
			  gint            nsegs)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  int i;

  if (!context)
    return;

  gdk_quartz_update_context_from_gc (context, gc);
  gdk_quartz_set_context_stroke_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						  _gdk_gc_get_fg_pixel (gc));

  for (i = 0; i < nsegs; i++)
    {
      CGContextMoveToPoint (context, segs[i].x1 + 0.5, segs[i].y1 + 0.5);
      CGContextAddLineToPoint (context, segs[i].x2 + 0.5, segs[i].y2 + 0.5);
    }
  
  CGContextStrokePath (context);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_lines (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  int i;

  if (!context)
    return;

  gdk_quartz_update_context_from_gc (context, gc);
  gdk_quartz_set_context_stroke_color_from_pixel (context, gdk_drawable_get_colormap (drawable),
						  _gdk_gc_get_fg_pixel (gc));
  
  for (i = 1; i < npoints; i++)
    {
      CGContextMoveToPoint (context, points[i - 1].x + 0.5, points[i - 1].y + 0.5);
      CGContextAddLineToPoint (context, points[i].x + 0.5, points[i].y + 0.5);
    }
  
  CGContextStrokePath (context);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_pixbuf (GdkDrawable     *drawable,
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
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGImageRef image;
  void *data;
  int rowstride, pixbuf_width, pixbuf_height;
  gboolean has_alpha;

  if (!context)
    return;

  pixbuf_width = gdk_pixbuf_get_width (pixbuf);
  pixbuf_height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  data = gdk_pixbuf_get_pixels (pixbuf);

  colorspace = CGColorSpaceCreateDeviceRGB ();
  data_provider = CGDataProviderCreateWithData (NULL, data, pixbuf_height * rowstride, NULL);

  image = CGImageCreate (pixbuf_width, pixbuf_height, 8,
			 has_alpha ? 32 : 24, rowstride, 
			 colorspace, 
			 has_alpha ? kCGImageAlphaLast : 0,
			 data_provider, NULL, FALSE, 
			 kCGRenderingIntentDefault);

  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  gdk_quartz_update_context_from_gc (context, gc);

  CGContextSetBlendMode (context, kCGBlendModeNormal);

  CGContextClipToRect (context, CGRectMake (dest_x, dest_y, width, height));
  CGContextTranslateCTM (context, dest_x - src_x, dest_y - src_y + pixbuf_height);
  CGContextScaleCTM (context, 1, -1);

  CGContextDrawImage (context, CGRectMake (0, 0, pixbuf_width, pixbuf_height), image);
  CGImageRelease (image);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_image (GdkDrawable     *drawable,
		       GdkGC           *gc,
		       GdkImage        *image,
		       gint             xsrc,
		       gint             ysrc,
		       gint             xdest,
		       gint             ydest,
		       gint             width,
		       gint             height)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGImageRef cgimage;

  if (!context)
    return;

  colorspace = CGColorSpaceCreateDeviceRGB ();
  data_provider = CGDataProviderCreateWithData (NULL, image->mem, image->height * image->bpl, NULL);

  /* FIXME: Make sure that this function draws 32-bit images correctly,
  * also check endianness wrt kCGImageAlphaNoneSkipFirst */
  cgimage = CGImageCreate (image->width, image->height, 8,
			   32, image->bpl,
			   colorspace,
			   kCGImageAlphaNoneSkipFirst, 
			   data_provider, NULL, FALSE, kCGRenderingIntentDefault);

  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  gdk_quartz_update_context_from_gc (context, gc);

  CGContextSetBlendMode (context, kCGBlendModeNormal);

  CGContextClipToRect (context, CGRectMake (xdest, ydest, width, height));
  CGContextTranslateCTM (context, xdest - xsrc, ydest - ysrc + image->height);
  CGContextScaleCTM (context, 1, -1);

  CGContextDrawImage (context, CGRectMake (0, 0, image->width, image->height), cgimage);
  CGImageRelease (cgimage);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_drawable_impl_quartz_finalize (GObject *object)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (object);

  if (impl->colormap)
    g_object_unref (impl->colormap);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_drawable_impl_quartz_class_init (GdkDrawableImplQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drawable_impl_quartz_finalize;

  drawable_class->create_gc = _gdk_quartz_gc_new;
  drawable_class->draw_rectangle = gdk_quartz_draw_rectangle;
  drawable_class->draw_arc = gdk_quartz_draw_arc;
  drawable_class->draw_polygon = gdk_quartz_draw_polygon;
  drawable_class->draw_text = gdk_quartz_draw_text;
  drawable_class->draw_text_wc = gdk_quartz_draw_text_wc;
  drawable_class->draw_drawable = gdk_quartz_draw_drawable;
  drawable_class->draw_points = gdk_quartz_draw_points;
  drawable_class->draw_segments = gdk_quartz_draw_segments;
  drawable_class->draw_lines = gdk_quartz_draw_lines;
  drawable_class->draw_image = gdk_quartz_draw_image;
  drawable_class->draw_pixbuf = gdk_quartz_draw_pixbuf;

  drawable_class->ref_cairo_surface = gdk_quartz_ref_cairo_surface;

  drawable_class->set_colormap = gdk_quartz_set_colormap;
  drawable_class->get_colormap = gdk_quartz_get_colormap;

  drawable_class->get_depth = gdk_quartz_get_depth;
  drawable_class->get_screen = gdk_quartz_get_screen;
  drawable_class->get_visual = gdk_quartz_get_visual;

  drawable_class->_copy_to_image = _gdk_quartz_copy_to_image;
}

GType
gdk_drawable_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDrawableImplQuartzClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_impl_quartz_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawableImplQuartz),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableImplQuartz",
                                            &object_info, 0);
    }
  
  return object_type;
}

CGContextRef 
gdk_quartz_drawable_get_context (GdkDrawable *drawable,
				 gboolean     antialias)
{
  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
    {
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (drawable);
      CGContextRef context;

      impl->pool = [[NSAutoreleasePool alloc] init];

      if (![impl->view lockFocusIfCanDraw])
	{
	  [impl->pool release];
	  return NULL;
	}

      context = [[NSGraphicsContext currentContext] graphicsPort];

      CGContextSaveGState (context);
      CGContextSetAllowsAntialiasing (context, antialias);

      /* We'll emulate the clipping caused by double buffering here */
      if (impl->begin_paint_count != 0)
	{
	  CGRect rect;
	  CGRect *cg_rects;
	  GdkRectangle *rects;
	  gint n_rects, i;
	  
	  gdk_region_get_rectangles (impl->paint_clip_region,
				     &rects, &n_rects);
	  
	  if (n_rects == 1)
	    cg_rects = &rect;
	  else
	    cg_rects = g_new (CGRect, n_rects);
	  
	  for (i = 0; i < n_rects; i++)
	    {
	      cg_rects[i].origin.x = rects[i].x;
	      cg_rects[i].origin.y = rects[i].y;
	      cg_rects[i].size.width = rects[i].width;
	      cg_rects[i].size.height = rects[i].height;
	    }
	  
	  CGContextClipToRects (context, cg_rects, n_rects);
	  
	  g_free (rects);
	  if (cg_rects != &rect)
	    g_free (cg_rects);
	}
      
      return context;
    }
  else if (GDK_IS_PIXMAP_IMPL_QUARTZ (drawable))
    {
      GdkPixmapImplQuartz *impl = GDK_PIXMAP_IMPL_QUARTZ (drawable);
      CGContextRef context;

      context = CGBitmapContextCreate (impl->data,
				       CGImageGetWidth (impl->image),
				       CGImageGetHeight (impl->image),
				       CGImageGetBitsPerComponent (impl->image),
				       CGImageGetBytesPerRow (impl->image),
				       CGImageGetColorSpace (impl->image),
				       CGImageGetBitmapInfo (impl->image));
      CGContextSetAllowsAntialiasing (context, antialias);
      
      return context;
    }

  g_assert_not_reached ();

  return NULL;
}

void
gdk_quartz_drawable_release_context (GdkDrawable  *drawable,
				     CGContextRef  context)
{
  if (!context)
    return;

  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
    {
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (drawable);

      CGContextRestoreGState (context);
      CGContextSetAllowsAntialiasing (context, TRUE);

      if (impl->in_paint_rect_count == 0 &&
	  impl->begin_paint_count == 0)
	CGContextFlush (context);

      [impl->view unlockFocus];
      [impl->pool release];
    }
  else if (GDK_IS_PIXMAP_IMPL_QUARTZ (drawable))
    {
      CGContextRelease (context);
    }
}
