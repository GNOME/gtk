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

#include "gdkprivate-x11.h"
#include "gdkregion-generic.h"

#include <pango/pangox.h>
#include <config.h>

#if HAVE_XFT
#include <pango/pangoxft.h>
#endif

#include <stdlib.h>

#if defined (HAVE_IPC_H) && defined (HAVE_SHM_H) && defined (HAVE_XSHM_H)
#define USE_SHM
#endif

#ifdef USE_SHM
#include <X11/extensions/XShm.h>
#endif /* USE_SHM */

#include "gdkprivate-x11.h"
#include "gdkdrawable-x11.h"
#include "gdkpixmap-x11.h"

static void gdk_x11_draw_rectangle (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gint            filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height);
static void gdk_x11_draw_arc       (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gint            filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height,
				    gint            angle1,
				    gint            angle2);
static void gdk_x11_draw_polygon   (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gint            filled,
				    GdkPoint       *points,
				    gint            npoints);
static void gdk_x11_draw_text      (GdkDrawable    *drawable,
				    GdkFont        *font,
				    GdkGC          *gc,
				    gint            x,
				    gint            y,
				    const gchar    *text,
				    gint            text_length);
static void gdk_x11_draw_text_wc   (GdkDrawable    *drawable,
				    GdkFont        *font,
				    GdkGC          *gc,
				    gint            x,
				    gint            y,
				    const GdkWChar *text,
				    gint            text_length);
static void gdk_x11_draw_drawable  (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPixmap      *src,
				    gint            xsrc,
				    gint            ysrc,
				    gint            xdest,
				    gint            ydest,
				    gint            width,
				    gint            height);
static void gdk_x11_draw_points    (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPoint       *points,
				    gint            npoints);
static void gdk_x11_draw_segments  (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkSegment     *segs,
				    gint            nsegs);
static void gdk_x11_draw_lines     (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPoint       *points,
				    gint            npoints);
static void gdk_x11_draw_glyphs    (GdkDrawable      *drawable,
                                    GdkGC            *gc,
                                    PangoFont        *font,
                                    gint              x,
                                    gint              y,
                                    PangoGlyphString *glyphs);
static void gdk_x11_draw_image     (GdkDrawable     *drawable,
                                    GdkGC           *gc,
                                    GdkImage        *image,
                                    gint             xsrc,
                                    gint             ysrc,
                                    gint             xdest,
                                    gint             ydest,
                                    gint             width,
                                    gint             height);

static void gdk_x11_set_colormap   (GdkDrawable    *drawable,
                                    GdkColormap    *colormap);

static GdkColormap* gdk_x11_get_colormap   (GdkDrawable    *drawable);

static gint         gdk_x11_get_depth      (GdkDrawable    *drawable);

static GdkVisual*   gdk_x11_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_x11_class_init (GdkDrawableImplX11Class *klass);

static void gdk_drawable_impl_x11_finalize   (GObject *object);

static gpointer parent_class = NULL;

GType
gdk_drawable_impl_x11_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDrawableImplX11Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_impl_x11_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawableImplX11),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableImplX11",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_drawable_impl_x11_class_init (GdkDrawableImplX11Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drawable_impl_x11_finalize;
  
  drawable_class->create_gc = _gdk_x11_gc_new;
  drawable_class->draw_rectangle = gdk_x11_draw_rectangle;
  drawable_class->draw_arc = gdk_x11_draw_arc;
  drawable_class->draw_polygon = gdk_x11_draw_polygon;
  drawable_class->draw_text = gdk_x11_draw_text;
  drawable_class->draw_text_wc = gdk_x11_draw_text_wc;
  drawable_class->draw_drawable = gdk_x11_draw_drawable;
  drawable_class->draw_points = gdk_x11_draw_points;
  drawable_class->draw_segments = gdk_x11_draw_segments;
  drawable_class->draw_lines = gdk_x11_draw_lines;
  drawable_class->draw_glyphs = gdk_x11_draw_glyphs;
  drawable_class->draw_image = gdk_x11_draw_image;
  
  drawable_class->set_colormap = gdk_x11_set_colormap;
  drawable_class->get_colormap = gdk_x11_get_colormap;

  drawable_class->get_depth = gdk_x11_get_depth;
  drawable_class->get_visual = gdk_x11_get_visual;
  
  drawable_class->get_image = _gdk_x11_get_image;
}

static void
gdk_drawable_impl_x11_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_x11_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  return impl->colormap;
}

static void
gdk_x11_set_colormap (GdkDrawable *drawable,
                      GdkColormap *colormap)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (impl->colormap == colormap)
    return;
  
  if (impl->colormap)
    gdk_colormap_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    gdk_colormap_ref (impl->colormap);
}

/* Drawing
 */

static void
gdk_x11_draw_rectangle (GdkDrawable *drawable,
			GdkGC       *gc,
			gint         filled,
			gint         x,
			gint         y,
			gint         width,
			gint         height)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (filled)
    XFillRectangle (impl->xdisplay, impl->xid,
		    GDK_GC_GET_XGC (gc), x, y, width, height);
  else
    XDrawRectangle (impl->xdisplay, impl->xid,
		    GDK_GC_GET_XGC (gc), x, y, width, height);
}

static void
gdk_x11_draw_arc (GdkDrawable *drawable,
		  GdkGC       *gc,
		  gint         filled,
		  gint         x,
		  gint         y,
		  gint         width,
		  gint         height,
		  gint         angle1,
		  gint         angle2)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  if (filled)
    XFillArc (impl->xdisplay, impl->xid,
	      GDK_GC_GET_XGC (gc), x, y, width, height, angle1, angle2);
  else
    XDrawArc (impl->xdisplay, impl->xid,
	      GDK_GC_GET_XGC (gc), x, y, width, height, angle1, angle2);
}

static void
gdk_x11_draw_polygon (GdkDrawable *drawable,
		      GdkGC       *gc,
		      gint         filled,
		      GdkPoint    *points,
		      gint         npoints)
{
  XPoint *tmp_points;
  gint tmp_npoints, i;
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  if (!filled &&
      (points[0].x != points[npoints-1].x || points[0].y != points[npoints-1].y))
    {
      tmp_npoints = npoints + 1;
      tmp_points = g_new (XPoint, tmp_npoints);
      tmp_points[npoints].x = points[0].x;
      tmp_points[npoints].y = points[0].y;
    }
  else
    {
      tmp_npoints = npoints;
      tmp_points = g_new (XPoint, tmp_npoints);
    }

  for (i=0; i<npoints; i++)
    {
      tmp_points[i].x = points[i].x;
      tmp_points[i].y = points[i].y;
    }
  
  if (filled)
    XFillPolygon (impl->xdisplay, impl->xid,
		  GDK_GC_GET_XGC (gc), tmp_points, tmp_npoints, Complex, CoordModeOrigin);
  else
    XDrawLines (impl->xdisplay, impl->xid,
		GDK_GC_GET_XGC (gc), tmp_points, tmp_npoints, CoordModeOrigin);

  g_free (tmp_points);
}

/* gdk_x11_draw_text
 *
 * Modified by Li-Da Lho to draw 16 bits and Multibyte strings
 *
 * Interface changed: add "GdkFont *font" to specify font or fontset explicitely
 */
static void
gdk_x11_draw_text (GdkDrawable *drawable,
		   GdkFont     *font,
		   GdkGC       *gc,
		   gint         x,
		   gint         y,
		   const gchar *text,
		   gint         text_length)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      XSetFont(impl->xdisplay, GDK_GC_GET_XGC (gc), xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawString (impl->xdisplay, impl->xid,
		       GDK_GC_GET_XGC (gc), x, y, text, text_length);
	}
      else
	{
	  XDrawString16 (impl->xdisplay, impl->xid,
			 GDK_GC_GET_XGC (gc), x, y, (XChar2b *) text, text_length / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) GDK_FONT_XFONT (font);
      XmbDrawString (impl->xdisplay, impl->xid,
		     fontset, GDK_GC_GET_XGC (gc), x, y, text, text_length);
    }
  else
    g_error("undefined font type\n");
}

static void
gdk_x11_draw_text_wc (GdkDrawable    *drawable,
		      GdkFont	     *font,
		      GdkGC	     *gc,
		      gint	      x,
		      gint	      y,
		      const GdkWChar *text,
		      gint	      text_length)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      gchar *text_8bit;
      gint i;
      XSetFont(impl->xdisplay, GDK_GC_GET_XGC (gc), xfont->fid);
      text_8bit = g_new (gchar, text_length);
      for (i=0; i<text_length; i++) text_8bit[i] = text[i];
      XDrawString (impl->xdisplay, impl->xid,
                   GDK_GC_GET_XGC (gc), x, y, text_8bit, text_length);
      g_free (text_8bit);
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      if (sizeof(GdkWChar) == sizeof(wchar_t))
	{
	  XwcDrawString (impl->xdisplay, impl->xid,
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_GET_XGC (gc), x, y, (wchar_t *)text, text_length);
	}
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  text_wchar = g_new (wchar_t, text_length);
	  for (i=0; i<text_length; i++) text_wchar[i] = text[i];
	  XwcDrawString (impl->xdisplay, impl->xid,
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_GET_XGC (gc), x, y, text_wchar, text_length);
	  g_free (text_wchar);
	}
    }
  else
    g_error("undefined font type\n");
}

static void
gdk_x11_draw_drawable (GdkDrawable *drawable,
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
  GdkDrawableImplX11 *impl;
  GdkDrawableImplX11 *src_impl;
  
  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (GDK_IS_DRAWABLE_IMPL_X11 (src))
    src_impl = GDK_DRAWABLE_IMPL_X11 (src);
  else
    src_impl = NULL;
  
  if (src_depth == 1)
    {
      XCopyArea (impl->xdisplay,
                 src_impl ? src_impl->xid : GDK_DRAWABLE_XID (src),
		 impl->xid,
		 GDK_GC_GET_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else if (dest_depth != 0 && src_depth == dest_depth)
    {
      XCopyArea (impl->xdisplay,
                 src_impl ? src_impl->xid : GDK_DRAWABLE_XID (src),
		 impl->xid,
		 GDK_GC_GET_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else
    g_warning ("Attempt to draw a drawable with depth %d to a drawable with depth %d",
               src_depth, dest_depth);
}

static void
gdk_x11_draw_points (GdkDrawable *drawable,
		     GdkGC       *gc,
		     GdkPoint    *points,
		     gint         npoints)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  /* We special-case npoints == 1, because X will merge multiple
   * consecutive XDrawPoint requests into a PolyPoint request
   */
  if (npoints == 1)
    {
      XDrawPoint (impl->xdisplay,
		  impl->xid,
		  GDK_GC_GET_XGC (gc),
		  points[0].x, points[0].y);
    }
  else
    {
      gint i;
      XPoint *tmp_points = g_new (XPoint, npoints);

      for (i=0; i<npoints; i++)
	{
	  tmp_points[i].x = points[i].x;
	  tmp_points[i].y = points[i].y;
	}
      
      XDrawPoints (impl->xdisplay,
		   impl->xid,
		   GDK_GC_GET_XGC (gc),
		   tmp_points,
		   npoints,
		   CoordModeOrigin);

      g_free (tmp_points);
    }
}

static void
gdk_x11_draw_segments (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkSegment  *segs,
		       gint         nsegs)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  /* We special-case nsegs == 1, because X will merge multiple
   * consecutive XDrawLine requests into a PolySegment request
   */
  if (nsegs == 1)
    {
      XDrawLine (impl->xdisplay, impl->xid,
		 GDK_GC_GET_XGC (gc), segs[0].x1, segs[0].y1,
		 segs[0].x2, segs[0].y2);
    }
  else
    {
      gint i;
      XSegment *tmp_segs = g_new (XSegment, nsegs);

      for (i=0; i<nsegs; i++)
	{
	  tmp_segs[i].x1 = segs[i].x1;
	  tmp_segs[i].x2 = segs[i].x2;
	  tmp_segs[i].y1 = segs[i].y1;
	  tmp_segs[i].y2 = segs[i].y2;
	}
      
      XDrawSegments (impl->xdisplay,
		     impl->xid,
		     GDK_GC_GET_XGC (gc),
		     tmp_segs, nsegs);

      g_free (tmp_segs);
    }
}

static void
gdk_x11_draw_lines (GdkDrawable *drawable,
		    GdkGC       *gc,
		    GdkPoint    *points,
		    gint         npoints)
{
  gint i;
  XPoint *tmp_points = g_new (XPoint, npoints);
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  for (i=0; i<npoints; i++)
    {
      tmp_points[i].x = points[i].x;
      tmp_points[i].y = points[i].y;
    }
      
  XDrawLines (impl->xdisplay,
	      impl->xid,
	      GDK_GC_GET_XGC (gc),
	      tmp_points, npoints,
	      CoordModeOrigin);

  g_free (tmp_points);
}

#if HAVE_XFT
static void
update_xft_draw_clip (GdkGC *gc)
{
  GdkGCX11 *private = GDK_GC_X11 (gc);
  int i;
  
  if (private->xft_draw)
    {
      if (private->clip_region)
	{
	  GdkRegionBox *boxes = private->clip_region->rects;
	  Region region = XCreateRegion ();
	  
	  for (i=0; i<private->clip_region->numRects; i++)
	    {
	      XRectangle rect;
	      
	      rect.x = CLAMP (boxes[i].x1 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT);
	      rect.y = CLAMP (boxes[i].y1 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT);
	      rect.width = CLAMP (boxes[i].x2 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT) - rect.x;
	      rect.height = CLAMP (boxes[i].y2 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT) - rect.y;
	      XUnionRectWithRegion (&rect, region, region);
	    }
	  
	  XftDrawSetClip (private->xft_draw, region);
	  XDestroyRegion (region);
	}
      else
	XftDrawSetClip (private->xft_draw, NULL);
    }
}
#endif  

static void
gdk_x11_draw_glyphs (GdkDrawable      *drawable,
		     GdkGC            *gc,
		     PangoFont        *font,
		     gint              x,
		     gint              y,
		     PangoGlyphString *glyphs)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

#if HAVE_XFT
  if (PANGO_XFT_IS_FONT (font))
    {
      GdkGCX11 *gc_x11 = GDK_GC_X11 (gc);
      XftColor xft_color;
      GdkColormap *cmap;
      GdkColor color;
      
      cmap = gdk_gc_get_colormap (gc);

      _gdk_x11_gc_flush (gc);
      
      if (!gc_x11->xft_draw)
	{
	  gc_x11->xft_draw = XftDrawCreate (impl->xdisplay,
					    impl->xid,
					    GDK_VISUAL_XVISUAL (gdk_colormap_get_visual (cmap)),
					    GDK_COLORMAP_XCOLORMAP (cmap));
	  update_xft_draw_clip (gc);
	}
      
      else
	{
	  XftDrawChange (gc_x11->xft_draw, impl->xid);
	  update_xft_draw_clip (gc);
	}
      
      gdk_colormap_query_color (cmap, gc_x11->fg_pixel, &color);
      
      xft_color.color.red = color.red;
      xft_color.color.green = color.green;
      xft_color.color.blue = color.blue;
      xft_color.color.alpha = 0xffff;
      
      pango_xft_render (gc_x11->xft_draw, &xft_color,
			font, glyphs, x, y);
    }
  else
#endif  /* !HAVE_XFT */
    pango_x_render (impl->xdisplay,
		    impl->xid,
		    GDK_GC_GET_XGC (gc),
		    font, glyphs, x, y);
}

static void
gdk_x11_draw_image     (GdkDrawable     *drawable,
                        GdkGC           *gc,
                        GdkImage        *image,
                        gint             xsrc,
                        gint             ysrc,
                        gint             xdest,
                        gint             ydest,
                        gint             width,
                        gint             height)
{
  GdkDrawableImplX11 *impl;
  
  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (image->type == GDK_IMAGE_SHARED)
    XShmPutImage (impl->xdisplay, impl->xid,
                  GDK_GC_GET_XGC (gc), GDK_IMAGE_XIMAGE (image),
                  xsrc, ysrc, xdest, ydest, width, height, False);
  else
    XPutImage (impl->xdisplay, impl->xid,
               GDK_GC_GET_XGC (gc), GDK_IMAGE_XIMAGE (image),
               xsrc, ysrc, xdest, ydest, width, height);
}

static gint
gdk_x11_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}

static GdkVisual*
gdk_x11_get_visual (GdkDrawable    *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}
