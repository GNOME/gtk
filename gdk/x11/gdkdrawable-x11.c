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

#include "gdkx.h"
#include "gdkregion-generic.h"

#include <pango/pangox.h>
#include <config.h>

#if HAVE_XFT
#include <pango/pangoxft.h>
#endif

#include <stdlib.h>
#include <string.h>		/* for memcpy() */

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
#ifdef HAVE_XFT
static void gdk_x11_draw_pixbuf    (GdkDrawable     *drawable,
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
#endif /* HAVE_XFT */

static void gdk_x11_set_colormap   (GdkDrawable    *drawable,
                                    GdkColormap    *colormap);

static GdkColormap* gdk_x11_get_colormap   (GdkDrawable    *drawable);

static gint         gdk_x11_get_depth      (GdkDrawable    *drawable);

static GdkVisual*   gdk_x11_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_x11_class_init (GdkDrawableImplX11Class *klass);

static void gdk_drawable_impl_x11_finalize   (GObject *object);

static gpointer parent_class = NULL;

GType
_gdk_drawable_impl_x11_get_type (void)
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
#ifdef HAVE_XFT  
  drawable_class->_draw_pixbuf = gdk_x11_draw_pixbuf;
#endif /* HAVE_XFT */
  
  drawable_class->set_colormap = gdk_x11_set_colormap;
  drawable_class->get_colormap = gdk_x11_get_colormap;

  drawable_class->get_depth = gdk_x11_get_depth;
  drawable_class->get_visual = gdk_x11_get_visual;
  
  drawable_class->_copy_to_image = _gdk_x11_copy_to_image;
}

static void
gdk_drawable_impl_x11_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#ifdef HAVE_XFT
gboolean
_gdk_x11_have_render (void)
{
  /* This check is cheap, but if we have to do version checks, we will
   * need to cache the result since version checks are round-trip
   */
  int event_base, error_base;

  return XRenderQueryExtension (gdk_display, &event_base, &error_base);
}

static Picture
gdk_x11_drawable_get_picture (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (impl->picture == None)
    {
      GdkVisual *visual = gdk_drawable_get_visual (drawable);
      XRenderPictFormat *format;

      if (!visual)
	{
	  g_warning ("Using Xft rendering requires the drawable argument to\n"
		     "have a specified colormap. All windows have a colormap,\n"
		     "however, pixmaps only have colormap by default if they\n"
		     "were created with a non-NULL window argument. Otherwise\n"
		     "a colormap must be set on them with gdk_drawable_set_colormap");
	  return None;
	}

      format = XRenderFindVisualFormat (impl->xdisplay, GDK_VISUAL_XVISUAL (visual));
      if (format)
	impl->picture = XRenderCreatePicture (impl->xdisplay, impl->xid, format, 0, NULL);
    }

  return impl->picture;
}

static void
gdk_x11_drawable_update_picture_clip (GdkDrawable *drawable,
				      GdkGC       *gc)
{
  GdkGCX11 *gc_private = gc ? GDK_GC_X11 (gc) : NULL;
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  Picture picture = gdk_x11_drawable_get_picture (drawable);

  if (gc && gc_private->clip_region)
    {
      GdkRegionBox *boxes = gc_private->clip_region->rects;
      gint n_boxes = gc_private->clip_region->numRects;
      XRectangle *rects = g_new (XRectangle, n_boxes);
      int i;

      for (i=0; i < n_boxes; i++)
	{
	  rects[i].x = CLAMP (boxes[i].x1 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT);
	  rects[i].y = CLAMP (boxes[i].y1 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT);
	  rects[i].width = CLAMP (boxes[i].x2 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT) - rects[i].x;
	  rects[i].height = CLAMP (boxes[i].y2 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT) - rects[i].y;
	}

      XRenderSetPictureClipRectangles (impl->xdisplay, picture, 0, 0, rects, n_boxes);

      g_free (rects);
    }
  else
    {
      XRenderPictureAttributes pa;
      pa.clip_mask = None;
      XRenderChangePicture (impl->xdisplay, picture, CPClipMask, &pa);
    }
}
#endif  

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
      Picture src_picture;
      Picture dest_picture;

      src_picture = _gdk_x11_gc_get_fg_picture (gc);

      gdk_x11_drawable_update_picture_clip (drawable, gc);
      dest_picture = gdk_x11_drawable_get_picture (drawable);
      
      pango_xft_picture_render (impl->xdisplay, src_picture, dest_picture, font, glyphs, x, y);
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

Display *
gdk_x11_drawable_get_xdisplay (GdkDrawable *drawable)
{
  GdkDrawable *impl;
  
  if (GDK_IS_WINDOW (drawable))
    impl = ((GdkPixmapObject *)drawable)->impl;
  else if (GDK_IS_PIXMAP (drawable))
    impl = ((GdkPixmapObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return NULL;
    }

  return ((GdkDrawableImplX11 *)impl)->xdisplay;
}

XID
gdk_x11_drawable_get_xid (GdkDrawable *drawable)
{
  GdkDrawable *impl;
  
  if (GDK_IS_WINDOW (drawable))
    impl = ((GdkPixmapObject *)drawable)->impl;
  else if (GDK_IS_PIXMAP (drawable))
    impl = ((GdkPixmapObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return None;
    }

  return ((GdkDrawableImplX11 *)impl)->xid;
}

/* Code for accelerated alpha compositing using the RENDER extension.
 * It's a bit long because there are lots of possibilities for
 * what's the fastest depending on the available picture formats,
 * whether we can used shared pixmaps, etc.
 */
#ifdef HAVE_XFT
typedef enum {
  FORMAT_NONE,
  FORMAT_EXACT_MASK,
  FORMAT_ARGB_MASK,
  FORMAT_ARGB
} FormatType;

static FormatType
select_format (Display            *xdisplay,
	       XRenderPictFormat **format,
	       XRenderPictFormat **mask)
{
  XRenderPictFormat pf;
  

/* Look for a 32-bit xRGB and Axxx formats that exactly match the
 * in memory data format. We can use them as pixmap and mask
 * to deal with non-premultiplied data.
 */

  pf.type = PictTypeDirect;
  pf.depth = 32;
  pf.direct.redMask = 0xff;
  pf.direct.greenMask = 0xff;
  pf.direct.blueMask = 0xff;
  
  pf.direct.alphaMask = 0;
  if (ImageByteOrder (xdisplay) == LSBFirst)
    {
      /* ABGR */
      pf.direct.red = 0;
      pf.direct.green = 8;
      pf.direct.blue = 16;
    }
  else
    {
      /* RGBA */
      pf.direct.red = 24;
      pf.direct.green = 16;
      pf.direct.blue = 8;
    }
  
  *format = XRenderFindFormat (xdisplay,
			       (PictFormatType | PictFormatDepth |
				PictFormatRedMask | PictFormatRed |
				PictFormatGreenMask | PictFormatGreen |
				PictFormatBlueMask | PictFormatBlue |
				PictFormatAlphaMask),
			       &pf,
			       0);

  pf.direct.alphaMask = 0xff;
  if (ImageByteOrder (xdisplay) == LSBFirst)
    {
      /* ABGR */
      pf.direct.alpha = 24;
    }
  else
    {
      pf.direct.alpha = 0;
    }
  
  *mask = XRenderFindFormat (xdisplay,
			     (PictFormatType | PictFormatDepth |
			      PictFormatAlphaMask | PictFormatAlpha),
			     &pf,
			     0);

  if (*format && *mask)
    return FORMAT_EXACT_MASK;

  /* OK, that failed, now look for xRGB and Axxx formats in
   * RENDER's preferred order
   */
  pf.direct.alphaMask = 0;
  /* ARGB */
  pf.direct.red = 16;
  pf.direct.green = 8;
  pf.direct.blue = 0;
  
  *format = XRenderFindFormat (xdisplay,
			       (PictFormatType | PictFormatDepth |
				PictFormatRedMask | PictFormatRed |
				PictFormatGreenMask | PictFormatGreen |
				PictFormatBlueMask | PictFormatBlue |
				PictFormatAlphaMask),
			       &pf,
			       0);

  pf.direct.alphaMask = 0xff;
  pf.direct.alpha = 24;
  
  *mask = XRenderFindFormat (xdisplay,
			     (PictFormatType | PictFormatDepth |
			      PictFormatAlphaMask | PictFormatAlpha),
			     &pf,
			     0);

  if (*format && *mask)
    return FORMAT_ARGB_MASK;

  /* Finally, if neither of the above worked, fall back to
   * looking for combined ARGB -- we'll premultiply ourselves.
   */

  pf.type = PictTypeDirect;
  pf.depth = 32;
  pf.direct.red = 16;
  pf.direct.green = 8;
  pf.direct.blue = 0;
  pf.direct.alphaMask = 0xff;
  pf.direct.alpha = 24;

  *format = XRenderFindFormat (xdisplay,
			       (PictFormatType | PictFormatDepth |
				PictFormatRedMask | PictFormatRed |
				PictFormatGreenMask | PictFormatGreen |
				PictFormatBlueMask | PictFormatBlue |
				PictFormatAlphaMask | PictFormatAlpha),
			       &pf,
			       0);
  *mask = NULL;

  if (*format)
    return FORMAT_ARGB;

  return FORMAT_NONE;
}

#if 0
static void
list_formats (XRenderPictFormat *pf)
{
  gint i;
  
  for (i=0 ;; i++)
    {
      XRenderPictFormat *pf = XRenderFindFormat (impl->xdisplay, 0, NULL, i);
      if (pf)
	{
	  g_print ("%2d R-%#06x/%#06x G-%#06x/%#06x B-%#06x/%#06x A-%#06x/%#06x\n",
		   pf->depth,
		   pf->direct.red,
		   pf->direct.redMask,
		   pf->direct.green,
		   pf->direct.greenMask,
		   pf->direct.blue,
		   pf->direct.blueMask,
		   pf->direct.alpha,
		   pf->direct.alphaMask);
	}
      else
	break;
    }
}
#endif  

static void
convert_to_format (guchar        *src_buf,
		   gint           src_rowstride,
		   guchar        *dest_buf,
		   gint           dest_rowstride,
		   FormatType     dest_format,
		   GdkByteOrder   dest_byteorder,
		   gint           width,
		   gint           height)
{
  gint i;

  if (dest_format == FORMAT_EXACT_MASK &&
      src_rowstride == dest_rowstride)
    {
      memcpy (dest_buf, src_buf, height * src_rowstride);
      return;
    }
  
  for (i=0; i < height; i++)
    {
      switch (dest_format)
	{
	case FORMAT_EXACT_MASK:
	  {
	    memcpy (dest_buf + i * dest_rowstride,
		    src_buf + i * src_rowstride,
		    width * 4);
	    break;
	  }
	case FORMAT_ARGB_MASK:
	  {
	    guint *p = (guint *)(src_buf + i * src_rowstride);
	    guint *q = (guint *)(dest_buf + i * dest_rowstride);
	    guint *end = p + width;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN	    
	    if (dest_byteorder == GDK_LSB_FIRST)
	      {
		/* ABGR => ARGB */
		
		while (p < end)
		  {
		    *q = ( (*p & 0xff00ff00) |
			  ((*p & 0x000000ff) << 16) |
			  ((*p & 0x00ff0000) >> 16));
		    q++;
		    p++;
		  }
	      }
	    else
	      {
		/* ABGR => BGRA */
		
		while (p < end)
		  {
		    *q = (((*p & 0xff000000) >> 24) |
			  ((*p & 0x00ffffff) << 8));
		    q++;
		    p++;
		  }
	      }
#else /* G_BYTE_ORDER == G_BIG_ENDIAN */
	    if (dest_byteorder == GDK_LSB_FIRST)
	      {
		/* RGBA => BGRA */
		
		while (p < end)
		  {
		    *q = ( (*p & 0x00ff00ff) |
			  ((*p & 0x0000ff00) << 16) |
			  ((*p & 0xff000000) >> 16));
		    q++;
		    p++;
		  }
	      }
	    else
	      {
		/* RGBA => ARGB */
		
		while (p < end)
		  {
		    *q = (((*p & 0xff000000) >> 24) |
			  ((*p & 0x00ffffff) << 8));
		    q++;
		    p++;
		  }
	      }
#endif /* G_BYTE_ORDER*/	    
	    break;
	  }
	case FORMAT_ARGB:
	  {
	    guchar *p = (src_buf + i * src_rowstride);
	    guchar *q = (dest_buf + i * dest_rowstride);
	    guchar *end = p + 4 * width;
	    guint t1,t2,t3;
	    
#define MULT(d,c,a,t) G_STMT_START { t = c * a; d = ((t >> 8) + t) >> 8; } G_STMT_END
	    
	    if (dest_byteorder == GDK_LSB_FIRST)
	      {
		while (p < end)
		  {
		    MULT(q[0], p[2], p[3], t1);
		    MULT(q[1], p[1], p[3], t2);
		    MULT(q[2], p[0], p[3], t3);
		    q[3] = p[3];
		    p += 4;
		    q += 4;
		  }
	      }
	    else
	      {
		while (p < end)
		  {
		    q[0] = p[3];
		    MULT(q[1], p[0], p[3], t1);
		    MULT(q[2], p[1], p[3], t2);
		    MULT(q[3], p[2], p[3], t3);
		    p += 4;
		    q += 4;
		  }
	      }
#undef MULT
	    break;
	  }
	case FORMAT_NONE:
	  g_assert_not_reached ();
	  break;
	}
    }
}

static void
draw_with_images (GdkDrawable       *drawable,
		  GdkGC             *gc,
		  FormatType         format_type,
		  XRenderPictFormat *format,
		  XRenderPictFormat *mask_format,
		  guchar            *src_rgb,
		  gint               src_rowstride,
		  gint               dest_x,
		  gint               dest_y,
		  gint               width,
		  gint               height)
{
  Display *xdisplay = GDK_DRAWABLE_IMPL_X11 (drawable)->xdisplay;
  GdkImage *image;
  GdkPixmap *pix;
  GdkGC *pix_gc;
  Picture pict;
  Picture dest_pict;
  Picture mask = None;
  gint x0, y0;

  pix = gdk_pixmap_new (NULL, width, height, 32);
  pict = XRenderCreatePicture (xdisplay, 
			       GDK_PIXMAP_XID (pix),
			       format, 0, NULL);
  if (mask_format)
    mask = XRenderCreatePicture (xdisplay, 
				 GDK_PIXMAP_XID (pix),
				 mask_format, 0, NULL);

  dest_pict = gdk_x11_drawable_get_picture (drawable);
  
  pix_gc = gdk_gc_new (pix);

  for (y0 = 0; y0 < height; y0 += GDK_SCRATCH_IMAGE_HEIGHT)
    {
      gint height1 = MIN (height - y0, GDK_SCRATCH_IMAGE_HEIGHT);
      for (x0 = 0; x0 < width; x0 += GDK_SCRATCH_IMAGE_WIDTH)
	{
	  gint xs0, ys0;
	  
	  gint width1 = MIN (width - x0, GDK_SCRATCH_IMAGE_WIDTH);
	  
	  image = _gdk_image_get_scratch (width1, height1, 32, &xs0, &ys0);
	  
	  convert_to_format (src_rgb + y0 * src_rowstride + 4 * x0, src_rowstride,
			     image->mem + ys0 * image->bpl + xs0 * image->bpp, image->bpl,
			     format_type, image->byte_order, 
			     width1, height1);

	  gdk_draw_image (pix, pix_gc,
			  image, xs0, ys0, x0, y0, width1, height1);
	}
    }
  
  XRenderComposite (xdisplay, PictOpOver, pict, mask, dest_pict, 
		    0, 0, 0, 0, dest_x, dest_y, width, height);

  XRenderFreePicture (xdisplay, pict);
  if (mask)
    XRenderFreePicture (xdisplay, mask);
  
  g_object_unref (pix);
  g_object_unref (pix_gc);
}

typedef struct _ShmPixmapInfo ShmPixmapInfo;

struct _ShmPixmapInfo
{
  GdkImage *image;
  Pixmap    pix;
  Picture   pict;
  Picture   mask;
};

/* Returns FALSE if we can't get a shm pixmap */
static gboolean
get_shm_pixmap_for_image (Display           *xdisplay,
			  GdkImage          *image,
			  XRenderPictFormat *format,
			  XRenderPictFormat *mask_format,
			  Pixmap            *pix,
			  Picture           *pict,
			  Picture           *mask)
{
  ShmPixmapInfo *info;
  
  if (image->type != GDK_IMAGE_SHARED)
    return FALSE;
  
  info = g_object_get_data (G_OBJECT (image), "gdk-x11-shm-pixmap");
  if (!info)
    {
      *pix = _gdk_x11_image_get_shm_pixmap (image);
      
      if (!*pix)
	return FALSE;
      
      info = g_new (ShmPixmapInfo, 1);
      info->pix = *pix;
      
      info->pict = XRenderCreatePicture (xdisplay, info->pix,
					 format, 0, NULL);
      if (mask_format)
	info->mask = XRenderCreatePicture (xdisplay, info->pix,
					   mask_format, 0, NULL);
      else
	info->mask = None;

      g_object_set_data (G_OBJECT (image), "gdk-x11-shm-pixmap", info);
    }

  *pix = info->pix;
  *pict = info->pict;
  *mask = info->mask;

  return TRUE;
}

#ifdef USE_SHM
/* Returns FALSE if drawing with ShmPixmaps is not possible */
static gboolean
draw_with_pixmaps (GdkDrawable       *drawable,
		   GdkGC             *gc,
		   FormatType         format_type,
		   XRenderPictFormat *format,
		   XRenderPictFormat *mask_format,
		   guchar            *src_rgb,
		   gint               src_rowstride,
		   gint               dest_x,
		   gint               dest_y,
		   gint               width,
		   gint               height)
{
  Display *xdisplay = GDK_DRAWABLE_IMPL_X11 (drawable)->xdisplay;
  GdkImage *image;
  Pixmap pix;
  Picture pict;
  Picture dest_pict;
  Picture mask = None;
  gint x0, y0;

  dest_pict = gdk_x11_drawable_get_picture (drawable);
  
  for (y0 = 0; y0 < height; y0 += GDK_SCRATCH_IMAGE_HEIGHT)
    {
      gint height1 = MIN (height - y0, GDK_SCRATCH_IMAGE_HEIGHT);
      for (x0 = 0; x0 < width; x0 += GDK_SCRATCH_IMAGE_WIDTH)
	{
	  gint xs0, ys0;
	  
	  gint width1 = MIN (width - x0, GDK_SCRATCH_IMAGE_WIDTH);
	  
	  image = _gdk_image_get_scratch (width1, height1, 32, &xs0, &ys0);
	  if (!get_shm_pixmap_for_image (xdisplay, image, format, mask_format, &pix, &pict, &mask))
	    return FALSE;

	  convert_to_format (src_rgb + y0 * src_rowstride + 4 * x0, src_rowstride,
			     image->mem + ys0 * image->bpl + xs0 * image->bpp, image->bpl,
			     format_type, image->byte_order, 
			     width1, height1);

	  XRenderComposite (xdisplay, PictOpOver, pict, mask, dest_pict, 
			    xs0, ys0, xs0, ys0, x0 + dest_x, y0 + dest_y,
			    width1, height1);
	}
    }

  return TRUE;
}
#endif

static void
gdk_x11_draw_pixbuf (GdkDrawable     *drawable,
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
  Display *xdisplay = GDK_DRAWABLE_IMPL_X11 (drawable)->xdisplay;
  FormatType format_type;
  XRenderPictFormat *format, *mask_format;
  gint rowstride;
#ifdef USE_SHM  
  gboolean use_pixmaps = TRUE;
#endif /* USE_SHM */
    
  format_type = select_format (xdisplay, &format, &mask_format);

  if (format_type == FORMAT_NONE ||
      !gdk_pixbuf_get_has_alpha (pixbuf) ||
      (dither == GDK_RGB_DITHER_MAX && gdk_drawable_get_depth (drawable) != 24))
    {
      GdkDrawable *wrapper = GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper;
      GDK_DRAWABLE_CLASS (parent_class)->_draw_pixbuf (wrapper, gc, pixbuf,
						       src_x, src_y, dest_x, dest_y,
						       width, height,
						       dither, x_dither, y_dither);
      return;
    }

  gdk_x11_drawable_update_picture_clip (drawable, gc);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

#ifdef USE_SHM
  if (use_pixmaps)
    {
      if (!draw_with_pixmaps (drawable, gc,
			      format_type, format, mask_format,
			      gdk_pixbuf_get_pixels (pixbuf) + src_y * rowstride + src_x * 4,
			      rowstride,
			      dest_x, dest_y, width, height))
	use_pixmaps = FALSE;
    }

  if (!use_pixmaps)
#endif /* USE_SHM */
    draw_with_images (drawable, gc,
		      format_type, format, mask_format,
		      gdk_pixbuf_get_pixels (pixbuf) + src_y * rowstride + src_x * 4,
		      rowstride,
		      dest_x, dest_y, width, height);
}
#endif /* HAVE_XFT */
