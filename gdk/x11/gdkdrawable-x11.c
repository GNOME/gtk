/* GIMP Drawing Kit
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

#include "gdkalias.h"
#include "gdkx.h"
#include "gdkregion-generic.h"

#include <pango/pangoxft.h>

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
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

static void gdk_x11_draw_rectangle (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gboolean        filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height);
static void gdk_x11_draw_arc       (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gboolean        filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height,
				    gint            angle1,
				    gint            angle2);
static void gdk_x11_draw_polygon   (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gboolean        filled,
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

static void gdk_x11_draw_glyphs             (GdkDrawable      *drawable,
					     GdkGC            *gc,
					     PangoFont        *font,
					     gint              x,
					     gint              y,
					     PangoGlyphString *glyphs);
static void gdk_x11_draw_glyphs_transformed (GdkDrawable      *drawable,
					     GdkGC            *gc,
					     PangoMatrix      *matrix,
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

static void gdk_x11_draw_trapezoids (GdkDrawable     *drawable,
				     GdkGC	     *gc,
				     GdkTrapezoid    *trapezoids,
				     gint             n_trapezoids);

static void gdk_x11_set_colormap   (GdkDrawable    *drawable,
                                    GdkColormap    *colormap);

static GdkColormap* gdk_x11_get_colormap   (GdkDrawable    *drawable);
static gint         gdk_x11_get_depth      (GdkDrawable    *drawable);
static GdkScreen *  gdk_x11_get_screen	   (GdkDrawable    *drawable);
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
  drawable_class->draw_glyphs_transformed = gdk_x11_draw_glyphs_transformed;
  drawable_class->draw_image = gdk_x11_draw_image;
  drawable_class->draw_pixbuf = gdk_x11_draw_pixbuf;
  drawable_class->draw_trapezoids = gdk_x11_draw_trapezoids;
  
  drawable_class->set_colormap = gdk_x11_set_colormap;
  drawable_class->get_colormap = gdk_x11_get_colormap;

  drawable_class->get_depth = gdk_x11_get_depth;
  drawable_class->get_screen = gdk_x11_get_screen;
  drawable_class->get_visual = gdk_x11_get_visual;
  
  drawable_class->_copy_to_image = _gdk_x11_copy_to_image;
}

static void
gdk_drawable_impl_x11_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
try_pixmap (Display *xdisplay,
	    int      screen,
	    int      depth)
{
  Pixmap pixmap = XCreatePixmap (xdisplay,
				 RootWindow (xdisplay, screen),
				 1, 1, depth);
  XFreePixmap (xdisplay, pixmap);
}

gboolean
_gdk_x11_have_render (GdkDisplay *display)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  GdkDisplayX11 *x11display = GDK_DISPLAY_X11 (display);

  if (x11display->have_render == GDK_UNKNOWN)
    {
      int event_base, error_base;
      x11display->have_render =
	XRenderQueryExtension (xdisplay, &event_base, &error_base)
	? GDK_YES : GDK_NO;

      if (x11display->have_render == GDK_YES)
	{
	  /*
	   * Sun advertises RENDER, but fails to support 32-bit pixmaps.
	   * That is just no good.  Therefore, we check all screens
	   * for proper support.
	   */

	  int screen;
	  for (screen = 0; screen < ScreenCount (xdisplay); screen++)
	    {
	      int count;
	      int *depths = XListDepths (xdisplay, screen, &count);
	      gboolean has_8 = FALSE, has_32 = FALSE;

	      if (depths)
		{
		  int i;

		  for (i = 0; i < count; i++)
		    {
		      if (depths[i] == 8)
			has_8 = TRUE;
		      else if (depths[i] == 32)
			has_32 = TRUE;
		    }
		  XFree (depths);
		}

	      /* At this point, we might have a false positive;
	       * buggy versions of Xinerama only report depths for
	       * which there is an associated visual; so we actually
	       * go ahead and try create pixmaps.
	       */
	      if (!(has_8 && has_32))
		{
		  gdk_error_trap_push ();
		  if (!has_8)
		    try_pixmap (xdisplay, screen, 8);
		  if (!has_32)
		    try_pixmap (xdisplay, screen, 32);
		  XSync (xdisplay, False);
		  if (gdk_error_trap_pop () == 0)
		    {
		      has_8 = TRUE;
		      has_32 = TRUE;
		    }
		}
	      
	      if (!(has_8 && has_32))
		{
		  g_warning ("The X server advertises that RENDER support is present,\n"
			     "but fails to supply the necessary pixmap support.  In\n"
			     "other words, it is buggy.");
		  x11display->have_render = GDK_NO;
		  break;
		}
	    }
	}
    }

  return x11display->have_render == GDK_YES;
}

gboolean
_gdk_x11_have_render_with_trapezoids (GdkDisplay *display)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  GdkDisplayX11 *x11display = GDK_DISPLAY_X11 (display);

  if (x11display->have_render_with_trapezoids == GDK_UNKNOWN)
    {
      x11display->have_render_with_trapezoids = GDK_NO;
      if (_gdk_x11_have_render (display))
	{
	  /*
	   * Require protocol >= 0.4 for CompositeTrapezoids support.
	   */
	  int major_version, minor_version;
	
#define XRENDER_TETRAPEZOIDS_MAJOR 0
#define XRENDER_TETRAPEZOIDS_MINOR 4
	
	  if (XRenderQueryVersion (xdisplay, &major_version,
				   &minor_version))
	    if ((major_version == XRENDER_TETRAPEZOIDS_MAJOR) &&
		(minor_version >= XRENDER_TETRAPEZOIDS_MINOR))
	      x11display->have_render_with_trapezoids = GDK_YES;
	}
    }

  return x11display->have_render_with_trapezoids == GDK_YES;
}

static XftDraw *
gdk_x11_drawable_get_xft_draw (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);

   if (impl->xft_draw == NULL)
    {
      GdkColormap *colormap = gdk_drawable_get_colormap (drawable);
      
      if (colormap)
	{
          GdkVisual *visual;

          visual = gdk_colormap_get_visual (colormap);
      
          impl->xft_draw = XftDrawCreate (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
 				          GDK_VISUAL_XVISUAL (visual), GDK_COLORMAP_XCOLORMAP (colormap));
	}
      else if (gdk_drawable_get_depth (drawable) == 1)
	{
	  impl->xft_draw = XftDrawCreateBitmap (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid);
	}
      else
        {
	  g_warning ("Using Xft rendering requires the drawable argument to\n"
		     "have a specified colormap. All windows have a colormap,\n"
		     "however, pixmaps only have colormap by default if they\n"
		     "were created with a non-NULL window argument. Otherwise\n"
		     "a colormap must be set on them with gdk_drawable_set_colormap");
	  return NULL;
        }
    }

   return impl->xft_draw;
}

static Picture
gdk_x11_drawable_get_picture (GdkDrawable *drawable)
{
  XftDraw *draw = gdk_x11_drawable_get_xft_draw (drawable);

  return draw ? XftDrawPicture (draw) : None;
}

static void
gdk_x11_drawable_update_xft_clip (GdkDrawable *drawable,
				   GdkGC       *gc)
{
  GdkGCX11 *gc_private = gc ? GDK_GC_X11 (gc) : NULL;
  XftDraw *xft_draw = gdk_x11_drawable_get_xft_draw (drawable);

  if (gc && gc_private->clip_region)
    {
      GdkRegionBox *boxes = gc_private->clip_region->rects;
      gint n_boxes = gc_private->clip_region->numRects;
#if 0				/* Until XftDrawSetClipRectangles is there */
      XRectangle *rects = g_new (XRectangle, n_boxes);
      int i;

      for (i=0; i < n_boxes; i++)
	{
	  rects[i].x = CLAMP (boxes[i].x1 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT);
	  rects[i].y = CLAMP (boxes[i].y1 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT);
	  rects[i].width = CLAMP (boxes[i].x2 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT) - rects[i].x;
	  rects[i].height = CLAMP (boxes[i].y2 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT) - rects[i].y;
	}
      XftDrawSetClipRectangles (xft_draw, 0, 0, rects, n_boxes);

      g_free (rects);
#else
      Region xregion = XCreateRegion ();
      int i;
 
      for (i=0; i < n_boxes; i++)
 	{
 	  XRectangle rect;
 	  
 	  rect.x = CLAMP (boxes[i].x1 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT);
 	  rect.y = CLAMP (boxes[i].y1 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT);
 	  rect.width = CLAMP (boxes[i].x2 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT) - rect.x;
 	  rect.height = CLAMP (boxes[i].y2 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT) - rect.y;
	  
 	  XUnionRectWithRegion (&rect, xregion, xregion);
 	}
      
      XftDrawSetClip (xft_draw, xregion);
      XDestroyRegion (xregion);
#endif      
    }
  else
    {
      XftDrawSetClip (xft_draw, NULL);
    }
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
    g_object_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    g_object_ref (impl->colormap);
}

/* Drawing
 */

static void
gdk_x11_draw_rectangle (GdkDrawable *drawable,
			GdkGC       *gc,
			gboolean     filled,
			gint         x,
			gint         y,
			gint         width,
			gint         height)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (filled)
    XFillRectangle (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		    GDK_GC_GET_XGC (gc), x, y, width, height);
  else
    XDrawRectangle (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		    GDK_GC_GET_XGC (gc), x, y, width, height);
}

static void
gdk_x11_draw_arc (GdkDrawable *drawable,
		  GdkGC       *gc,
		  gboolean     filled,
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
    XFillArc (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
	      GDK_GC_GET_XGC (gc), x, y, width, height, angle1, angle2);
  else
    XDrawArc (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
	      GDK_GC_GET_XGC (gc), x, y, width, height, angle1, angle2);
}

static void
gdk_x11_draw_polygon (GdkDrawable *drawable,
		      GdkGC       *gc,
		      gboolean     filled,
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
    XFillPolygon (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		  GDK_GC_GET_XGC (gc), tmp_points, tmp_npoints, Complex, CoordModeOrigin);
  else
    XDrawLines (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
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
  Display *xdisplay;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  xdisplay = GDK_SCREEN_XDISPLAY (impl->screen);
  
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      XSetFont(xdisplay, GDK_GC_GET_XGC (gc), xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawString (xdisplay, impl->xid,
		       GDK_GC_GET_XGC (gc), x, y, text, text_length);
	}
      else
	{
	  XDrawString16 (xdisplay, impl->xid,
			 GDK_GC_GET_XGC (gc), x, y, (XChar2b *) text, text_length / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) GDK_FONT_XFONT (font);
      XmbDrawString (xdisplay, impl->xid,
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
  Display *xdisplay;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  xdisplay = GDK_SCREEN_XDISPLAY (impl->screen);
  
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      gchar *text_8bit;
      gint i;
      XSetFont(xdisplay, GDK_GC_GET_XGC (gc), xfont->fid);
      text_8bit = g_new (gchar, text_length);
      for (i=0; i<text_length; i++) text_8bit[i] = text[i];
      XDrawString (xdisplay, impl->xid,
                   GDK_GC_GET_XGC (gc), x, y, text_8bit, text_length);
      g_free (text_8bit);
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      if (sizeof(GdkWChar) == sizeof(wchar_t))
	{
	  XwcDrawString (xdisplay, impl->xid,
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_GET_XGC (gc), x, y, (wchar_t *)text, text_length);
	}
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  text_wchar = g_new (wchar_t, text_length);
	  for (i=0; i<text_length; i++) text_wchar[i] = text[i];
	  XwcDrawString (xdisplay, impl->xid,
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
      XCopyArea (GDK_SCREEN_XDISPLAY (impl->screen),
                 src_impl ? src_impl->xid : GDK_DRAWABLE_XID (src),
		 impl->xid,
		 GDK_GC_GET_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else if (dest_depth != 0 && src_depth == dest_depth)
    {
      XCopyArea (GDK_SCREEN_XDISPLAY (impl->screen),
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
      XDrawPoint (GDK_SCREEN_XDISPLAY (impl->screen),
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
      
      XDrawPoints (GDK_SCREEN_XDISPLAY (impl->screen),
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
      XDrawLine (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
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
      
      XDrawSegments (GDK_SCREEN_XDISPLAY (impl->screen),
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
      
  XDrawLines (GDK_SCREEN_XDISPLAY (impl->screen),
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
  gdk_x11_draw_glyphs_transformed (drawable, gc, NULL,
				   font,
				   x * PANGO_SCALE,
				   y * PANGO_SCALE,
				   glyphs);
}

static void
gdk_x11_draw_glyphs_transformed (GdkDrawable      *drawable,
				 GdkGC            *gc,
				 PangoMatrix      *matrix,
				 PangoFont        *font,
				 gint              x,
				 gint              y,
				 PangoGlyphString *glyphs)
{
  GdkDrawableImplX11 *impl;
  PangoRenderer *renderer;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  g_return_if_fail (PANGO_XFT_IS_FONT (font));

  renderer = _gdk_x11_renderer_get (drawable, gc);
  if (matrix)
    pango_renderer_set_matrix (renderer, matrix);
  pango_renderer_draw_glyphs (renderer, font, glyphs, x, y);
  if (matrix)
    pango_renderer_set_matrix (renderer, NULL);
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

#ifdef USE_SHM  
  if (image->type == GDK_IMAGE_SHARED)
    XShmPutImage (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
                  GDK_GC_GET_XGC (gc), GDK_IMAGE_XIMAGE (image),
                  xsrc, ysrc, xdest, ydest, width, height, False);
  else
#endif
    XPutImage (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
               GDK_GC_GET_XGC (gc), GDK_IMAGE_XIMAGE (image),
               xsrc, ysrc, xdest, ydest, width, height);
}

static gint
gdk_x11_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}

static GdkDrawable *
get_impl_drawable (GdkDrawable *drawable)
{
  if (GDK_IS_WINDOW (drawable))
    return ((GdkWindowObject *)drawable)->impl;
  else if (GDK_IS_PIXMAP (drawable))
    return ((GdkPixmapObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return NULL;
    }
}

static GdkScreen*
gdk_x11_get_screen (GdkDrawable *drawable)
{
  if (GDK_IS_DRAWABLE_IMPL_X11 (drawable))
    return GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  else
    return GDK_DRAWABLE_IMPL_X11 (get_impl_drawable (drawable))->screen;
}

static GdkVisual*
gdk_x11_get_visual (GdkDrawable    *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}

/**
 * gdk_x11_drawable_get_xdisplay:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the display of a #GdkDrawable.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_drawable_get_xdisplay (GdkDrawable *drawable)
{
  if (GDK_IS_DRAWABLE_IMPL_X11 (drawable))
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (drawable)->screen);
  else
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (get_impl_drawable (drawable))->screen);
}

/**
 * gdk_x11_drawable_get_xid:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the X resource (window or pixmap) belonging to a #GdkDrawable.
 * 
 * Return value: the ID of @drawable's X resource.
 **/
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

static GdkX11FormatType
select_format (GdkDisplay         *display,
	       XRenderPictFormat **format,
	       XRenderPictFormat **mask)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  XRenderPictFormat pf;

  if (!_gdk_x11_have_render (display))
    return GDK_X11_FORMAT_NONE;
  
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
    return GDK_X11_FORMAT_EXACT_MASK;

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
    return GDK_X11_FORMAT_ARGB_MASK;

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
    return GDK_X11_FORMAT_ARGB;

  return GDK_X11_FORMAT_NONE;
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

void
_gdk_x11_convert_to_format (guchar           *src_buf,
                            gint              src_rowstride,
                            guchar           *dest_buf,
                            gint              dest_rowstride,
                            GdkX11FormatType  dest_format,
                            GdkByteOrder      dest_byteorder,
                            gint              width,
                            gint              height)
{
  gint i;

  for (i=0; i < height; i++)
    {
      switch (dest_format)
	{
	case GDK_X11_FORMAT_EXACT_MASK:
	  {
	    memcpy (dest_buf + i * dest_rowstride,
		    src_buf + i * src_rowstride,
		    width * 4);
	    break;
	  }
	case GDK_X11_FORMAT_ARGB_MASK:
	  {
	    guchar *row = src_buf + i * src_rowstride;
	    if (((gsize)row & 3) != 0)
	      {
		guchar *p = row;
		guint32 *q = (guint32 *)(dest_buf + i * dest_rowstride);
		guchar *end = p + 4 * width;

		while (p < end)
		  {
		    *q = (p[3] << 24) | (p[0] << 16) | (p[1] << 8) | p[2];
		    p += 4;
		    q++;
		  }
	      }
	    else
	      {
		guint32 *p = (guint32 *)row;
		guint32 *q = (guint32 *)(dest_buf + i * dest_rowstride);
		guint32 *end = p + width;

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
			*q = (((*p & 0xffffff00) >> 8) |
			      ((*p & 0x000000ff) << 24));
			q++;
			p++;
		      }
		  }
#endif /* G_BYTE_ORDER*/	    
	      }
	    break;
	  }
	case GDK_X11_FORMAT_ARGB:
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
	case GDK_X11_FORMAT_NONE:
	  g_assert_not_reached ();
	  break;
	}
    }
}

static void
draw_with_images (GdkDrawable       *drawable,
		  GdkGC             *gc,
		  GdkX11FormatType   format_type,
		  XRenderPictFormat *format,
		  XRenderPictFormat *mask_format,
		  guchar            *src_rgb,
		  gint               src_rowstride,
		  gint               dest_x,
		  gint               dest_y,
		  gint               width,
		  gint               height)
{
  GdkScreen *screen = GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  Display *xdisplay = GDK_SCREEN_XDISPLAY (screen);
  GdkImage *image;
  GdkPixmap *pix;
  GdkGC *pix_gc;
  Picture pict;
  Picture dest_pict;
  Picture mask = None;
  gint x0, y0;

  pix = gdk_pixmap_new (gdk_screen_get_root_window (screen), width, height, 32);
						  
  pict = XRenderCreatePicture (xdisplay, 
			       GDK_PIXMAP_XID (pix),
			       format, 0, NULL);
  if (mask_format)
    mask = XRenderCreatePicture (xdisplay, 
				 GDK_PIXMAP_XID (pix),
				 mask_format, 0, NULL);

  dest_pict = gdk_x11_drawable_get_picture (drawable);  
  
  pix_gc = _gdk_drawable_get_scratch_gc (pix, FALSE);

  for (y0 = 0; y0 < height; y0 += GDK_SCRATCH_IMAGE_HEIGHT)
    {
      gint height1 = MIN (height - y0, GDK_SCRATCH_IMAGE_HEIGHT);
      for (x0 = 0; x0 < width; x0 += GDK_SCRATCH_IMAGE_WIDTH)
	{
	  gint xs0, ys0;
	  
	  gint width1 = MIN (width - x0, GDK_SCRATCH_IMAGE_WIDTH);
	  
	  image = _gdk_image_get_scratch (screen, width1, height1, 32, &xs0, &ys0);
	  
	  _gdk_x11_convert_to_format (src_rgb + y0 * src_rowstride + 4 * x0, src_rowstride,
                                      (guchar *)image->mem + ys0 * image->bpl + xs0 * image->bpp, image->bpl,
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
		   GdkX11FormatType   format_type,
		   XRenderPictFormat *format,
		   XRenderPictFormat *mask_format,
		   guchar            *src_rgb,
		   gint               src_rowstride,
		   gint               dest_x,
		   gint               dest_y,
		   gint               width,
		   gint               height)
{
  Display *xdisplay = GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (drawable)->screen);
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
	  
	  image = _gdk_image_get_scratch (GDK_DRAWABLE_IMPL_X11 (drawable)->screen,
					  width1, height1, 32, &xs0, &ys0);
	  if (!get_shm_pixmap_for_image (xdisplay, image, format, mask_format, &pix, &pict, &mask))
	    return FALSE;

	  _gdk_x11_convert_to_format (src_rgb + y0 * src_rowstride + 4 * x0, src_rowstride,
                                      (guchar *)image->mem + ys0 * image->bpl + xs0 * image->bpp, image->bpl,
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
  GdkX11FormatType format_type;
  XRenderPictFormat *format, *mask_format;
  gint rowstride;
#ifdef USE_SHM  
  gboolean use_pixmaps = TRUE;
#endif /* USE_SHM */
    
  format_type = select_format (gdk_drawable_get_display (drawable),
			       &format, &mask_format);

  if (format_type == GDK_X11_FORMAT_NONE ||
      !gdk_pixbuf_get_has_alpha (pixbuf) ||
      gdk_drawable_get_depth (drawable) == 1 ||
      (dither == GDK_RGB_DITHER_MAX && gdk_drawable_get_depth (drawable) != 24) ||
      gdk_x11_drawable_get_picture (drawable) == None)
    {
      GdkDrawable *wrapper = GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper;
      GDK_DRAWABLE_CLASS (parent_class)->draw_pixbuf (wrapper, gc, pixbuf,
						      src_x, src_y, dest_x, dest_y,
						      width, height,
						      dither, x_dither, y_dither);
      return;
    }

  gdk_x11_drawable_update_xft_clip (drawable, gc);

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

static void
gdk_x11_draw_trapezoids (GdkDrawable  *drawable,
			 GdkGC	      *gc,
			 GdkTrapezoid *trapezoids,
			 gint          n_trapezoids)
{
  GdkScreen *screen = GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  GdkDisplay *display = gdk_screen_get_display (screen);
  XTrapezoid *xtrapezoids;
  gint i;

  if (!_gdk_x11_have_render_with_trapezoids (display))
    {
      GdkDrawable *wrapper = GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper;
      GDK_DRAWABLE_CLASS (parent_class)->draw_trapezoids (wrapper, gc,
							  trapezoids, n_trapezoids);
      return;
    }

  xtrapezoids = g_new (XTrapezoid, n_trapezoids);

  for (i = 0; i < n_trapezoids; i++)
    {
      xtrapezoids[i].top = XDoubleToFixed (trapezoids[i].y1);
      xtrapezoids[i].bottom = XDoubleToFixed (trapezoids[i].y2);
      xtrapezoids[i].left.p1.x = XDoubleToFixed (trapezoids[i].x11);
      xtrapezoids[i].left.p1.y = XDoubleToFixed (trapezoids[i].y1);
      xtrapezoids[i].left.p2.x = XDoubleToFixed (trapezoids[i].x12);
      xtrapezoids[i].left.p2.y = XDoubleToFixed (trapezoids[i].y2);
      xtrapezoids[i].right.p1.x = XDoubleToFixed (trapezoids[i].x21);
      xtrapezoids[i].right.p1.y = XDoubleToFixed (trapezoids[i].y1);
      xtrapezoids[i].right.p2.x = XDoubleToFixed (trapezoids[i].x22);
      xtrapezoids[i].right.p2.y = XDoubleToFixed (trapezoids[i].y2);
    }

  _gdk_x11_drawable_draw_xtrapezoids (drawable, gc,
				      xtrapezoids, n_trapezoids);
  
  g_free (xtrapezoids);
}

/**
 * gdk_draw_rectangle_alpha_libgtk_only:
 * @drawable: The #GdkDrawable to draw on
 * @x: the x coordinate of the left edge of the rectangle.
 * @y: the y coordinate of the top edge of the rectangle.
 * @width: the width of the rectangle.
 * @height: the height of the rectangle.
 * @color: The color
 * @alpha: The alpha value.
 * 
 * Tries to draw a filled alpha blended rectangle using the window
 * system's native routines. This is not public API and must not be
 * used by applications.
 * 
 * Return value: TRUE if the rectangle could be drawn, FALSE
 * otherwise.
 **/
gboolean
gdk_draw_rectangle_alpha_libgtk_only (GdkDrawable  *drawable,
				      gint          x,
				      gint          y,
				      gint          width,
				      gint          height,
				      GdkColor     *color,
				      guint16       alpha)
{
  Display *xdisplay;
  XRenderColor render_color;
  Picture pict;
  int x_offset, y_offset;
  GdkDrawable *real_drawable, *impl;

  g_return_val_if_fail (color != NULL, FALSE);
  
  if (!GDK_IS_WINDOW (drawable))
    return FALSE;
  
  if (!_gdk_x11_have_render (gdk_drawable_get_display (drawable)))
    return FALSE;

  gdk_window_get_internal_paint_info (GDK_WINDOW (drawable),
				      &real_drawable,
				      &x_offset, &y_offset);

  impl = ((GdkWindowObject *)real_drawable)->impl;  
      
  pict = gdk_x11_drawable_get_picture (impl);

  if (pict == None)
    return FALSE;
  
  xdisplay = GDK_DISPLAY_XDISPLAY (gdk_drawable_get_display (drawable));

  render_color.alpha = alpha;
  render_color.red = (guint32)color->red * render_color.alpha / 0xffff;
  render_color.green = (guint32)color->green * render_color.alpha / 0xffff;
  render_color.blue = (guint32)color->blue * render_color.alpha / 0xffff;

  XRenderFillRectangle (xdisplay,
			PictOpOver, pict, &render_color,
			x - x_offset, y - y_offset,
			width, height);
  return TRUE;
}

void
_gdk_x11_drawable_draw_xtrapezoids (GdkDrawable      *drawable,
				    GdkGC            *gc,
				    XTrapezoid       *xtrapezoids,
				    int               n_trapezoids)
{
  GdkScreen *screen = GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkDisplayX11 *x11display = GDK_DISPLAY_X11 (display);
   
  XftDraw *draw;

  if (!_gdk_x11_have_render_with_trapezoids (display))
    {
      /* This is the case of drawing the borders of the unknown glyph box
       * without render on the display, we need to feed it back to
       * fallback code. Not efficient, but doesn't matter.
       */
      GdkTrapezoid *trapezoids = g_new (GdkTrapezoid, n_trapezoids);
      int i;

      for (i = 0; i < n_trapezoids; i++)
	{
	  trapezoids[i].y1 = XFixedToDouble (xtrapezoids[i].top);
	  trapezoids[i].y2 = XFixedToDouble (xtrapezoids[i].bottom);
	  trapezoids[i].x11 = XFixedToDouble (xtrapezoids[i].left.p1.x);
	  trapezoids[i].x12 = XFixedToDouble (xtrapezoids[i].left.p2.x);
	  trapezoids[i].x21 = XFixedToDouble (xtrapezoids[i].right.p1.x);
	  trapezoids[i].x22 = XFixedToDouble (xtrapezoids[i].right.p2.x);
	}

      gdk_x11_draw_trapezoids (drawable, gc, trapezoids, n_trapezoids);
      g_free (trapezoids);

      return;
    }

  gdk_x11_drawable_update_xft_clip (drawable, gc);
  draw = gdk_x11_drawable_get_xft_draw (drawable);

  if (!x11display->mask_format)
    x11display->mask_format = XRenderFindStandardFormat (x11display->xdisplay,
							 PictStandardA8);

  XRenderCompositeTrapezoids (x11display->xdisplay, PictOpOver,
			      _gdk_x11_gc_get_fg_picture (gc),
			      XftDrawPicture (draw),
			      x11display->mask_format,
			      - gc->ts_x_origin, - gc->ts_y_origin,
			      xtrapezoids, n_trapezoids);
}

void
_gdk_x11_drawable_draw_xft_glyphs (GdkDrawable      *drawable,
				   GdkGC            *gc,
				   XftFont          *xft_font,
				   XftGlyphSpec     *glyphs,
				   gint              n_glyphs)
{
  GdkScreen *screen = GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkDisplayX11 *x11display = GDK_DISPLAY_X11 (display);
  XftDraw *draw;
   
  gdk_x11_drawable_update_xft_clip (drawable, gc);
  draw = gdk_x11_drawable_get_xft_draw (drawable);

  if (_gdk_x11_have_render (display))
    {
      XftGlyphSpecRender (x11display->xdisplay, PictOpOver,
			  _gdk_x11_gc_get_fg_picture (gc),
			  xft_font,
			  XftDrawPicture (draw),
			  - gc->ts_x_origin, - gc->ts_y_origin,
			  glyphs, n_glyphs);
    }
  else
    {
      XftColor color;
      
      _gdk_gc_x11_get_fg_xft_color (gc, &color);
      XftDrawGlyphSpec (draw, &color, xft_font, glyphs, n_glyphs);
    }
}
