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

#include <math.h>
#include <glib.h>

#ifndef G_PI
#define G_PI 3.14159265358979323846
#endif

#include "gdkdrawable.h"
#include "gdkprivate.h"
#include "gdkwindow.h"
#include "gdkwin32.h"

static void gdk_win32_drawable_destroy   (GdkDrawable     *drawable);

static void gdk_win32_draw_rectangle (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gint            filled,
				      gint            x,
				      gint            y,
				      gint            width,
				      gint            height);
static void gdk_win32_draw_arc       (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gint            filled,
				      gint            x,
				      gint            y,
				      gint            width,
				      gint            height,
				      gint            angle1,
				      gint            angle2);
static void gdk_win32_draw_polygon   (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gint            filled,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_text      (GdkDrawable    *drawable,
				      GdkFont        *font,
				      GdkGC          *gc,
				      gint            x,
				      gint            y,
				      const gchar    *text,
				      gint            text_length);
static void gdk_win32_draw_text_wc   (GdkDrawable    *drawable,
				      GdkFont        *font,
				      GdkGC          *gc,
				      gint            x,
				      gint            y,
				      const GdkWChar *text,
				      gint            text_length);
static void gdk_win32_draw_drawable  (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPixmap      *src,
				      gint            xsrc,
				      gint            ysrc,
				      gint            xdest,
				      gint            ydest,
				      gint            width,
				      gint            height);
static void gdk_win32_draw_points    (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_segments  (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkSegment     *segs,
				      gint            nsegs);
static void gdk_win32_draw_lines     (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPoint       *points,
				      gint            npoints);

GdkDrawableClass _gdk_win32_drawable_class = {
  gdk_win32_drawable_destroy,
  _gdk_win32_gc_new,
  gdk_win32_draw_rectangle,
  gdk_win32_draw_arc,
  gdk_win32_draw_polygon,
  gdk_win32_draw_text,
  gdk_win32_draw_text_wc,
  gdk_win32_draw_drawable,
  gdk_win32_draw_points,
  gdk_win32_draw_segments,
  gdk_win32_draw_lines
};

/*****************************************************
 * Win32 specific implementations of generic functions *
 *****************************************************/

GdkColormap*
gdk_drawable_get_colormap (GdkDrawable *drawable)
{
  GdkDrawablePrivate *drawable_private;
  
  g_return_val_if_fail (drawable != NULL, NULL);
  drawable_private = (GdkDrawablePrivate*) drawable;

  if (!GDK_DRAWABLE_DESTROYED (drawable))
    {
      if (drawable_private->colormap == NULL)
	return gdk_colormap_get_system (); /* XXX ??? */
      else
	return drawable_private->colormap;
    }
  
  return NULL;
}

void
gdk_drawable_set_colormap (GdkDrawable *drawable,
			   GdkColormap *colormap)
{
  GdkDrawablePrivate *drawable_private;
  GdkColormapPrivateWin32 *colormap_private;
  
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (colormap != NULL);
  
  drawable_private = (GdkDrawablePrivate *) drawable;
  colormap_private = (GdkColormapPrivateWin32 *) colormap;
  
  if (!GDK_DRAWABLE_DESTROYED (drawable))
    {
      if (GDK_IS_WINDOW (drawable))
	{
	  g_return_if_fail (colormap_private->visual !=
			    ((GdkColormapPrivate*)(drawable_private->colormap))->visual);
	  /* XXX ??? */
	  GDK_NOTE (MISC, g_print ("gdk_drawable_set_colormap: %#x %#x\n",
				   GDK_DRAWABLE_XID (drawable),
				   colormap_private->xcolormap));
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
gdk_win32_drawable_destroy (GdkDrawable *drawable)
{
  
}

static void
gdk_win32_draw_rectangle (GdkDrawable *drawable,
			  GdkGC       *gc,
			  gint         filled,
			  gint         x,
			  gint         y,
			  gint         width,
			  gint         height)
{
  GdkGCPrivate *gc_private;
  HDC hdc;
  HGDIOBJ oldpen, oldbrush;

  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable, gc_private);

  GDK_NOTE (MISC, g_print ("gdk_draw_rectangle: %#x (%d) %s%dx%d@+%d+%d\n",
			   GDK_DRAWABLE_XID (drawable),
			   gc_private,
			   (filled ? "fill " : ""),
			   width, height, x, y));
    
#if 0
  {
    HBRUSH hbr = GetCurrentObject (hdc, OBJ_BRUSH);
    HPEN hpen = GetCurrentObject (hdc, OBJ_PEN);
    LOGBRUSH lbr;
    LOGPEN lpen;
    GetObject (hbr, sizeof (lbr), &lbr);
    GetObject (hpen, sizeof (lpen), &lpen);
    
    g_print ("current brush: style = %s, color = 0x%.08x\n",
	     (lbr.lbStyle == BS_SOLID ? "SOLID" : "???"),
	     lbr.lbColor);
    g_print ("current pen: style = %s, width = %d, color = 0x%.08x\n",
	     (lpen.lopnStyle == PS_SOLID ? "SOLID" : "???"),
	     lpen.lopnWidth,
	     lpen.lopnColor);
  }
#endif

  if (filled)
    oldpen = SelectObject (hdc, GetStockObject (NULL_PEN));
  else
    oldbrush = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
  
  if (!Rectangle (hdc, x, y, x+width+1, y+height+1))
    g_warning ("gdk_draw_rectangle: Rectangle failed");
  
  if (filled)
    SelectObject (hdc, oldpen);
  else
    SelectObject (hdc, oldbrush);

  gdk_gc_postdraw (drawable, gc_private);
}

static void
gdk_win32_draw_arc (GdkDrawable *drawable,
		    GdkGC       *gc,
		    gint         filled,
		    gint         x,
		    gint         y,
		    gint         width,
		    gint         height,
		    gint         angle1,
		    gint         angle2)
{
  GdkGCPrivate *gc_private;
  HDC hdc;
  int nXStartArc, nYStartArc, nXEndArc, nYEndArc;

  gc_private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_draw_arc: %#x  %d,%d,%d,%d  %d %d\n",
			   GDK_DRAWABLE_XID (drawable),
			   x, y, width, height, angle1, angle2));

  if (width != 0 && height != 0 && angle2 != 0)
    {
      hdc = gdk_gc_predraw (drawable, gc_private);

      if (angle2 >= 360*64)
	{
	  nXStartArc = nYStartArc = nXEndArc = nYEndArc = 0;
	}
      else if (angle2 > 0)
	{
	  /* The 100. is just an arbitrary value */
	  nXStartArc = x + width/2 + 100. * cos(angle1/64.*2.*G_PI/360.);
	  nYStartArc = y + height/2 + -100. * sin(angle1/64.*2.*G_PI/360.);
	  nXEndArc = x + width/2 + 100. * cos((angle1+angle2)/64.*2.*G_PI/360.);
	  nYEndArc = y + height/2 + -100. * sin((angle1+angle2)/64.*2.*G_PI/360.);
	}
      else
	{
	  nXEndArc = x + width/2 + 100. * cos(angle1/64.*2.*G_PI/360.);
	  nYEndArc = y + height/2 + -100. * sin(angle1/64.*2.*G_PI/360.);
	  nXStartArc = x + width/2 + 100. * cos((angle1+angle2)/64.*2.*G_PI/360.);
	  nYStartArc = y + height/2 + -100. * sin((angle1+angle2)/64.*2.*G_PI/360.);
	}

      if (filled)
	{
	  GDK_NOTE (MISC, g_print ("...Pie(hdc,%d,%d,%d,%d,%d,%d,%d,%d)\n",
				   x, y, x+width, y+height,
				   nXStartArc, nYStartArc,
				   nXEndArc, nYEndArc));
	  Pie (hdc, x, y, x+width, y+height,
	       nXStartArc, nYStartArc, nXEndArc, nYEndArc);
	}
      else
	{
	  GDK_NOTE (MISC, g_print ("...Arc(hdc,%d,%d,%d,%d,%d,%d,%d,%d)\n",
				   x, y, x+width, y+height,
				   nXStartArc, nYStartArc,
				   nXEndArc, nYEndArc));
	  Arc (hdc, x, y, x+width, y+height,
	       nXStartArc, nYStartArc, nXEndArc, nYEndArc);
	}
      gdk_gc_postdraw (drawable, gc_private);
    }
}

static void
gdk_win32_draw_polygon (GdkDrawable *drawable,
			GdkGC       *gc,
			gint         filled,
			GdkPoint    *points,
			gint         npoints)
{
  GdkGCPrivate *gc_private;
  HDC hdc;
  POINT *pts;
  int i;

  gc_private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_draw_polygon: %#x (%d) %d\n",
			   GDK_DRAWABLE_XID (drawable), gc_private,
			   npoints));

  if (npoints < 2)
    return;

  hdc = gdk_gc_predraw (drawable, gc_private);
  pts = g_malloc ((npoints+1) * sizeof (POINT));

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if ((points[0].x != points[npoints-1].x) ||
      (points[0].y != points[npoints-1].y)) 
    {
      pts[npoints].x = points[0].x;
      pts[npoints].y = points[0].y;
      npoints++;
    }
  if (filled)
    {
      if (!Polygon (hdc, pts, npoints))
	g_warning ("gdk_draw_polygon: Polygon failed");
    }
  else
    {
      if (!Polyline (hdc, pts, npoints))
	g_warning ("gdk_draw_polygon: Polyline failed");
    }
  g_free (pts);
  gdk_gc_postdraw (drawable, gc_private);
}

typedef struct
{
  gint x, y;
  HDC hdc;
} gdk_draw_text_arg;

static void
gdk_draw_text_handler (GdkWin32SingleFont *singlefont,
		       const wchar_t      *wcstr,
		       int                 wclen,
		       void               *arg)
{
  HGDIOBJ oldfont;
  SIZE size;
  gdk_draw_text_arg *argp = (gdk_draw_text_arg *) arg;

  if (!singlefont)
    return;

  if ((oldfont = SelectObject (argp->hdc, singlefont->xfont)) == NULL)
    {
      g_warning ("gdk_draw_text_handler: SelectObject failed");
      return;
    }
  
  if (!TextOutW (argp->hdc, argp->x, argp->y, wcstr, wclen))
    g_warning ("gdk_draw_text_handler: TextOutW failed");
  GetTextExtentPoint32W (argp->hdc, wcstr, wclen, &size);
  argp->x += size.cx;

  SelectObject (argp->hdc, oldfont);
}

static void
gdk_win32_draw_text (GdkDrawable *drawable,
		     GdkFont     *font,
		     GdkGC       *gc,
		     gint         x,
		     gint         y,
		     const gchar *text,
		     gint         text_length)
{
  GdkGCPrivate *gc_private;
  wchar_t *wcstr;
  gint wlen;
  gdk_draw_text_arg arg;


  if (GDK_DRAWABLE_DESTROYED (drawable))
    return;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  gc_private = (GdkGCPrivate*) gc;

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_gc_predraw (drawable, gc_private);

  GDK_NOTE (MISC, g_print ("gdk_draw_text: %#x (%d,%d) \"%.*s\" (len %d)\n",
			   GDK_DRAWABLE_XID (drawable),
			   x, y,
			   (text_length > 10 ? 10 : text_length),
			   text, text_length));
  
  wcstr = g_new (wchar_t, text_length);
  if ((wlen = gdk_nmbstowchar_ts (wcstr, text, text_length, text_length)) == -1)
    g_warning ("gdk_draw_text: gdk_nmbstowchar_ts failed");
  else
    gdk_wchar_text_handle (font, wcstr, wlen,
			   gdk_draw_text_handler, &arg);

  g_free (wcstr);

  gdk_gc_postdraw (drawable, gc_private);
}

static void
gdk_win32_draw_text_wc (GdkDrawable	 *drawable,
			GdkFont          *font,
			GdkGC		 *gc,
			gint		  x,
			gint		  y,
			const GdkWChar *text,
			gint		  text_length)
{
  GdkGCPrivate *gc_private;
  gint i, wlen;
  wchar_t *wcstr;
  gdk_draw_text_arg arg;


  if (GDK_DRAWABLE_DESTROYED (drawable))
    return;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  gc_private = (GdkGCPrivate*) gc;

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_gc_predraw (drawable, gc_private);

  GDK_NOTE (MISC, g_print ("gdk_draw_text_wc: %#x (%d,%d) len: %d\n",
			   GDK_DRAWABLE_XID (drawable),
			   x, y, text_length));
      
  if (sizeof (wchar_t) != sizeof (GdkWChar))
    {
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
    }
  else
    wcstr = (wchar_t *) text;

  gdk_wchar_text_handle (font, wcstr, text_length,
			 gdk_draw_text_handler, &arg);

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    g_free (wcstr);

  gdk_gc_postdraw (drawable, gc_private);
}

static void
gdk_win32_draw_drawable (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkPixmap   *src,
			 gint         xsrc,
			 gint         ysrc,
			 gint         xdest,
			 gint         ydest,
			 gint         width,
			 gint         height)
{
  GdkDrawablePrivate *src_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  HDC srcdc;
  HGDIOBJ hgdiobj;
  HRGN src_rgn, draw_rgn, outside_rgn;
  RECT r;

  src_private = (GdkDrawablePrivate*) src;
  gc_private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_draw_pixmap: dest: %#x "
			   "src: %#x %dx%d@+%d+%d"
			   " dest: %#x @+%d+%d\n",
			   GDK_DRAWABLE_XID (drawable),
			   GDK_DRAWABLE_XID (src),
			   width, height, xsrc, ysrc,
			   GDK_DRAWABLE_XID (drawable), xdest, ydest));

  hdc = gdk_gc_predraw (drawable, gc_private);

  src_rgn = CreateRectRgn (0, 0, src_private->width + 1, src_private->height + 1);
  draw_rgn = CreateRectRgn (xsrc, ysrc, xsrc + width + 1, ysrc + height + 1);
  SetRectEmpty (&r);
  outside_rgn = CreateRectRgnIndirect (&r);
  
  if (GDK_DRAWABLE_TYPE (drawable) != GDK_DRAWABLE_PIXMAP)
    {
      /* If we are drawing on a window, calculate the region that is
       * outside the source pixmap, and invalidate that, causing it to
       * be cleared. XXX
       */
      if (CombineRgn (outside_rgn, draw_rgn, src_rgn, RGN_DIFF) != NULLREGION)
	{
	  OffsetRgn (outside_rgn, xdest, ydest);
	  GDK_NOTE (MISC, (GetRgnBox (outside_rgn, &r),
			   g_print ("...calling InvalidateRgn, "
				    "bbox: %dx%d@+%d+%d\n",
				    r.right - r.left - 1, r.bottom - r.top - 1,
				    r.left, r.top)));
	  InvalidateRgn (GDK_DRAWABLE_XID (drawable), outside_rgn, TRUE);
	}
    }

#if 1 /* Don't know if this is necessary  */
  if (CombineRgn (draw_rgn, draw_rgn, src_rgn, RGN_AND) == COMPLEXREGION)
    g_warning ("gdk_draw_pixmap: CombineRgn returned a COMPLEXREGION");

  GetRgnBox (draw_rgn, &r);
  if (r.left != xsrc
      || r.top != ysrc
      || r.right != xsrc + width + 1
      || r.bottom != ysrc + height + 1)
    {
      xdest += r.left - xsrc;
      xsrc = r.left;
      ydest += r.top - ysrc;
      ysrc = r.top;
      width = r.right - xsrc - 1;
      height = r.bottom - ysrc - 1;
      
      GDK_NOTE (MISC, g_print ("... restricted to src: %dx%d@+%d+%d, "
			       "dest: @+%d+%d\n",
			       width, height, xsrc, ysrc,
			       xdest, ydest));
    }
#endif

  DeleteObject (src_rgn);
  DeleteObject (draw_rgn);
  DeleteObject (outside_rgn);

  /* Strangely enough, this function is called also to bitblt
   * from a window.
   */
  if (src_private->window_type == GDK_DRAWABLE_PIXMAP)
    {
      if ((srcdc = CreateCompatibleDC (hdc)) == NULL)
	g_warning ("gdk_draw_pixmap: CreateCompatibleDC failed");
      
      if ((hgdiobj = SelectObject (srcdc, GDK_DRAWABLE_XID (src))) == NULL)
	g_warning ("gdk_draw_pixmap: SelectObject #1 failed");
      
      if (!BitBlt (hdc, xdest, ydest, width, height,
		   srcdc, xsrc, ysrc, SRCCOPY))
	g_warning ("gdk_draw_pixmap: BitBlt failed");
      
      if ((SelectObject (srcdc, hgdiobj) == NULL))
	g_warning ("gdk_draw_pixmap: SelectObject #2 failed");
      
      if (!DeleteDC (srcdc))
	g_warning ("gdk_draw_pixmap: DeleteDC failed");
    }
  else
    {
      if (GDK_DRAWABLE_XID(drawable) == GDK_DRAWABLE_XID (src))
	{
	  /* Blitting inside a window, use ScrollDC */
	  RECT scrollRect, clipRect, emptyRect;
	  HRGN updateRgn;

	  scrollRect.left = MIN (xsrc, xdest);
	  scrollRect.top = MIN (ysrc, ydest);
	  scrollRect.right = MAX (xsrc + width + 1, xdest + width + 1);
	  scrollRect.bottom = MAX (ysrc + height + 1, ydest + height + 1);

	  clipRect.left = xdest;
	  clipRect.top = ydest;
	  clipRect.right = xdest + width + 1;
	  clipRect.bottom = ydest + height + 1;

	  SetRectEmpty (&emptyRect);
	  updateRgn = CreateRectRgnIndirect (&emptyRect);
	  if (!ScrollDC (hdc, xdest - xsrc, ydest - ysrc,
			 &scrollRect, &clipRect,
			 updateRgn, NULL))
	    g_warning ("gdk_draw_pixmap: ScrollDC failed");
	  if (!InvalidateRgn (GDK_DRAWABLE_XID (drawable), updateRgn, FALSE))
	    g_warning ("gdk_draw_pixmap: InvalidateRgn failed");
	  if (!UpdateWindow (GDK_DRAWABLE_XID (drawable)))
	    g_warning ("gdk_draw_pixmap: UpdateWindow failed");
	}
      else
	{
	  if ((srcdc = GetDC (GDK_DRAWABLE_XID (src))) == NULL)
	    g_warning ("gdk_draw_pixmap: GetDC failed");
	  
	  if (!BitBlt (hdc, xdest, ydest, width, height,
		       srcdc, xsrc, ysrc, SRCCOPY))
	    g_warning ("gdk_draw_pixmap: BitBlt failed");
	  ReleaseDC (GDK_DRAWABLE_XID (src), srcdc);
	}
    }
  gdk_gc_postdraw (drawable, gc_private);
}

static void
gdk_win32_draw_points (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  GdkGCPrivate *gc_private;
  HDC hdc;
  int i;

  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable, gc_private);
  
  GDK_NOTE (MISC, g_print ("gdk_draw_points: %#x destdc: (%d) %#x "
			   "npoints: %d\n",
			   GDK_DRAWABLE_XID (drawable),
			   gc_private, hdc,
			   npoints));

  for (i = 0; i < npoints; i++)
    {
      if (!MoveToEx (hdc, points[i].x, points[i].y, NULL))
	g_warning ("gdk_draw_points: MoveToEx failed");
      if (!LineTo (hdc, points[i].x + 1, points[i].y))
	g_warning ("gdk_draw_points: LineTo failed");
    }
  gdk_gc_postdraw (drawable, gc_private);
}

static void
gdk_win32_draw_segments (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkSegment  *segs,
			 gint         nsegs)
{
  GdkGCPrivate *gc_private;
  HDC hdc;
  int i;

  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable, gc_private);

  for (i = 0; i < nsegs; i++)
    {
      if (!MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
	g_warning ("gdk_draw_segments: MoveToEx failed");
      if (!LineTo (hdc, segs[i].x2, segs[i].y2))
	g_warning ("gdk_draw_segments: LineTo #1 failed");
      
      /* Draw end pixel */
      if (GDK_GC_WIN32DATA (gc)->pen_width == 1)
	if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
	  g_warning ("gdk_draw_segments: LineTo #2 failed");
    }
  gdk_gc_postdraw (drawable, gc_private);
}

static void
gdk_win32_draw_lines (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkPoint    *points,
		      gint         npoints)
{
  GdkGCPrivate *gc_private;
  HDC hdc;
  POINT *pts;
  int i;

  if (npoints < 2)
    return;

  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable, gc_private);
#if 1
  pts = g_malloc (npoints * sizeof (POINT));

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if (!Polyline (hdc, pts, npoints))
    g_warning ("gdk_draw_lines: Polyline(,,%d) failed", npoints);
  
  g_free (pts);
  
  /* Draw end pixel */
  if (GDK_GC_WIN32DATA (gc)->pen_width == 1)
    {
      MoveToEx (hdc, points[npoints-1].x, points[npoints-1].y, NULL);
      if (!LineTo (hdc, points[npoints-1].x + 1, points[npoints-1].y))
	g_warning ("gdk_draw_lines: LineTo failed");
    }
#else
  MoveToEx (hdc, points[0].x, points[0].y, NULL);
  for (i = 1; i < npoints; i++)
    if (!LineTo (hdc, points[i].x, points[i].y))
      g_warning ("gdk_draw_lines: LineTo #1 failed");
  
  /* Draw end pixel */
  /* LineTo doesn't draw the last point, so if we have a pen width of 1,
   * we draw the end pixel separately... With wider pens we don't care.
   * //HB: But the NT developers don't read their API documentation ...
   */
  if (GDK_GC_WIN32DATA (gc)->pen_width == 1 && windows_version > 0x80000000)
    if (!LineTo (hdc, points[npoints-1].x + 1, points[npoints-1].y))
      g_warning ("gdk_draw_lines: LineTo #2 failed");
#endif	
  gdk_gc_postdraw (drawable, gc_private);
}
