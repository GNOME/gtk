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

#include "gdkprivate-x11.h"

static void    gdk_x11_drawable_destroy   (GdkDrawable     *drawable);

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


GdkDrawableClass _gdk_x11_drawable_class = {
  gdk_x11_drawable_destroy,
  _gdk_x11_gc_new,
  gdk_x11_draw_rectangle,
  gdk_x11_draw_arc,
  gdk_x11_draw_polygon,
  gdk_x11_draw_text,
  gdk_x11_draw_text_wc,
  gdk_x11_draw_drawable,
  gdk_x11_draw_points,
  gdk_x11_draw_segments,
  gdk_x11_draw_lines
};

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

GdkColormap*
gdk_drawable_get_colormap (GdkDrawable *drawable)
{
  GdkDrawablePrivate *drawable_private;
  XWindowAttributes window_attributes;
  
  g_return_val_if_fail (drawable != NULL, NULL);
  drawable_private = (GdkDrawablePrivate*) drawable;
  
  if (!GDK_DRAWABLE_DESTROYED (drawable))
    {
      if (drawable_private->colormap == NULL &&
	  GDK_IS_WINDOW (drawable))
	{
	  XGetWindowAttributes (GDK_DRAWABLE_XDISPLAY (drawable),
				GDK_DRAWABLE_XID (drawable),
				&window_attributes);
	  drawable_private->colormap =  gdk_colormap_lookup (window_attributes.colormap);
	}

      return drawable_private->colormap;
    }
  
  return NULL;
}

void
gdk_drawable_set_colormap (GdkDrawable *drawable,
			   GdkColormap *colormap)
{
  GdkDrawablePrivate *drawable_private;
  GdkColormapPrivateX *colormap_private;
  
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (colormap != NULL);
  
  drawable_private = (GdkDrawablePrivate *)drawable;
  colormap_private = (GdkColormapPrivateX *)colormap;
  
  if (!GDK_DRAWABLE_DESTROYED (drawable))
    {
      if (GDK_IS_WINDOW (drawable))
	{
	  g_return_if_fail (colormap_private->base.visual !=
			    ((GdkColormapPrivate *)(drawable_private->colormap))->visual);

	  XSetWindowColormap (GDK_DRAWABLE_XDISPLAY (drawable),
			      GDK_DRAWABLE_XID (drawable),
			      colormap_private->xcolormap);
	}

      if (drawable_private->colormap)
	gdk_colormap_unref (drawable_private->colormap);
      drawable_private->colormap = colormap;
      gdk_colormap_ref (drawable_private->colormap);

      if (GDK_IS_WINDOW (drawable) &&
	  drawable_private->window_type != GDK_WINDOW_TOPLEVEL)
	gdk_window_add_colormap_windows (drawable);
    }
}

/* Drawing
 */
static void 
gdk_x11_drawable_destroy (GdkDrawable *drawable)
{
  
}

static void
gdk_x11_draw_rectangle (GdkDrawable *drawable,
			GdkGC       *gc,
			gint         filled,
			gint         x,
			gint         y,
			gint         width,
			gint         height)
{
  if (filled)
    XFillRectangle (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		    GDK_GC_GET_XGC (gc), x, y, width, height);
  else
    XDrawRectangle (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
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
  if (filled)
    XFillArc (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
	      GDK_GC_GET_XGC (gc), x, y, width, height, angle1, angle2);
  else
    XDrawArc (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
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
    XFillPolygon (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		  GDK_GC_GET_XGC (gc), tmp_points, tmp_npoints, Complex, CoordModeOrigin);
  else
    XDrawLines (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
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
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      XSetFont(GDK_DRAWABLE_XDISPLAY (drawable), GDK_GC_GET_XGC (gc), xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		       GDK_GC_GET_XGC (gc), x, y, text, text_length);
	}
      else
	{
	  XDrawString16 (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
			 GDK_GC_GET_XGC (gc), x, y, (XChar2b *) text, text_length / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) GDK_FONT_XFONT (font);
      XmbDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
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
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      gchar *text_8bit;
      gint i;
      XSetFont(GDK_DRAWABLE_XDISPLAY (drawable), GDK_GC_GET_XGC (gc), xfont->fid);
      text_8bit = g_new (gchar, text_length);
      for (i=0; i<text_length; i++) text_8bit[i] = text[i];
      XDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
                   GDK_GC_GET_XGC (gc), x, y, text_8bit, text_length);
      g_free (text_8bit);
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      if (sizeof(GdkWChar) == sizeof(wchar_t))
	{
	  XwcDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_GET_XGC (gc), x, y, (wchar_t *)text, text_length);
	}
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  text_wchar = g_new (wchar_t, text_length);
	  for (i=0; i<text_length; i++) text_wchar[i] = text[i];
	  XwcDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
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

  if (src_depth == 1)
    {
      XCopyArea (GDK_DRAWABLE_XDISPLAY (drawable),
		 GDK_DRAWABLE_XID (src),
		 GDK_DRAWABLE_XID (drawable),
		 GDK_GC_GET_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else if (dest_depth != 0 && src_depth == dest_depth)
    {
      XCopyArea (GDK_DRAWABLE_XDISPLAY (drawable),
		 GDK_DRAWABLE_XID (src),
		 GDK_DRAWABLE_XID (drawable),
		 GDK_GC_GET_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else
    g_warning ("Attempt to copy between drawables of mismatched depths!\n");
}

static void
gdk_x11_draw_points (GdkDrawable *drawable,
		     GdkGC       *gc,
		     GdkPoint    *points,
		     gint         npoints)
{
  /* We special-case npoints == 1, because X will merge multiple
   * consecutive XDrawPoint requests into a PolyPoint request
   */
  if (npoints == 1)
    {
      XDrawPoint (GDK_DRAWABLE_XDISPLAY (drawable),
		  GDK_DRAWABLE_XID (drawable),
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
      
      XDrawPoints (GDK_DRAWABLE_XDISPLAY (drawable),
		   GDK_DRAWABLE_XID (drawable),
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
  /* We special-case nsegs == 1, because X will merge multiple
   * consecutive XDrawLine requests into a PolySegment request
   */
  if (nsegs == 1)
    {
      XDrawLine (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
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
      
      XDrawSegments (GDK_DRAWABLE_XDISPLAY (drawable),
		     GDK_DRAWABLE_XID (drawable),
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

  for (i=0; i<npoints; i++)
    {
      tmp_points[i].x = points[i].x;
      tmp_points[i].y = points[i].y;
    }
      
  XDrawLines (GDK_DRAWABLE_XDISPLAY (drawable),
	      GDK_DRAWABLE_XID (drawable),
	      GDK_GC_GET_XGC (gc),
	      tmp_points, npoints,
	      CoordModeOrigin);

  g_free (tmp_points);
}
