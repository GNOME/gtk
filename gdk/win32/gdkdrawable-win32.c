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

#include <math.h>
#include <glib.h>

#include <pango/pangowin32.h>

#include "gdkinternals.h"
#include "gdkprivate-win32.h"

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
static void gdk_win32_draw_glyphs    (GdkDrawable      *drawable,
				      GdkGC            *gc,
				      PangoFont        *font,
				      gint              x,
				      gint              y,
				      PangoGlyphString *glyphs);
static void gdk_win32_draw_image     (GdkDrawable     *drawable,
				      GdkGC           *gc,
				      GdkImage        *image,
				      gint             xsrc,
				      gint             ysrc,
				      gint             xdest,
				      gint             ydest,
				      gint             width,
				      gint             height);

static void gdk_win32_set_colormap   (GdkDrawable    *drawable,
				      GdkColormap    *colormap);

static GdkColormap* gdk_win32_get_colormap   (GdkDrawable    *drawable);

static gint         gdk_win32_get_depth      (GdkDrawable    *drawable);

static GdkVisual*   gdk_win32_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_win32_class_init (GdkDrawableImplWin32Class *klass);

static gpointer parent_class = NULL;

GType
gdk_drawable_impl_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDrawableImplWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_impl_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawableImplWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableImplWin32",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_drawable_impl_win32_class_init (GdkDrawableImplWin32Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  drawable_class->create_gc = _gdk_win32_gc_new;
  drawable_class->draw_rectangle = gdk_win32_draw_rectangle;
  drawable_class->draw_arc = gdk_win32_draw_arc;
  drawable_class->draw_polygon = gdk_win32_draw_polygon;
  drawable_class->draw_text = gdk_win32_draw_text;
  drawable_class->draw_text_wc = gdk_win32_draw_text_wc;
  drawable_class->draw_drawable = gdk_win32_draw_drawable;
  drawable_class->draw_points = gdk_win32_draw_points;
  drawable_class->draw_segments = gdk_win32_draw_segments;
  drawable_class->draw_lines = gdk_win32_draw_lines;
  drawable_class->draw_glyphs = gdk_win32_draw_glyphs;
  drawable_class->draw_image = gdk_win32_draw_image;
  
  drawable_class->set_colormap = gdk_win32_set_colormap;
  drawable_class->get_colormap = gdk_win32_get_colormap;

  drawable_class->get_depth = gdk_win32_get_depth;
  drawable_class->get_visual = gdk_win32_get_visual;

  drawable_class->get_image = _gdk_win32_get_image;
}

/*****************************************************
 * Win32 specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_win32_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl;
  
  impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  return impl->colormap;
}

static void
gdk_win32_set_colormap (GdkDrawable *drawable,
			GdkColormap *colormap)
{
  GdkDrawableImplWin32 *impl;

  g_return_if_fail (colormap != NULL);  

  impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

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
gdk_win32_draw_rectangle (GdkDrawable *drawable,
			  GdkGC       *gc,
			  gint         filled,
			  gint         x,
			  gint         y,
			  gint         width,
			  gint         height)
{
  GdkGCWin32 *gc_private = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  HGDIOBJ old_pen_or_brush;
  POINT pts[4];
  gboolean ok = TRUE;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_rectangle: %#x (%p) %s%dx%d@+%d+%d\n",
			   (guint) GDK_DRAWABLE_HANDLE (drawable),
			   gc_private,
			   (filled ? "fill " : ""),
			   width, height, x, y));
    
  hdc = gdk_win32_hdc_get (drawable, gc, mask);

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

  if (gc_private->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;

      /* Win9x doesn't support Rectangle calls in a path,
       * thus use Polyline.
       */
	  
      pts[0].x = x;
      pts[0].y = y;
      pts[1].x = x + width + 1;
      pts[1].y = y;
      pts[2].x = x + width + 1;
      pts[2].y = y + height + 1;
      pts[3].x = x;
      pts[3].y = y + height + 1;
      
      if (ok)
	MoveToEx (hdc, x, y, NULL);
      
      if (ok && !Polyline (hdc, pts, 4))
	WIN32_GDI_FAILED ("Polyline"), ok = FALSE;
	  
      if (ok && !CloseFigure (hdc))
	WIN32_GDI_FAILED ("CloseFigure"), ok = FALSE;

      if (ok && !EndPath (hdc))
	WIN32_GDI_FAILED ("EndPath"), ok = FALSE;
	  
      if (ok && !filled)
	if (!WidenPath (hdc))
	  WIN32_GDI_FAILED ("WidenPath"), ok = FALSE;
	  
      if (ok && !FillPath (hdc))
	WIN32_GDI_FAILED ("FillPath"), ok = FALSE;
    }
  else
    {
      if (filled)
	old_pen_or_brush = SelectObject (hdc, GetStockObject (NULL_PEN));
      else
	old_pen_or_brush = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
  
      if (!Rectangle (hdc, x, y, x+width+1, y+height+1))
	WIN32_GDI_FAILED ("Rectangle");
  
      SelectObject (hdc, old_pen_or_brush);
    }

  gdk_win32_hdc_release (drawable, gc, mask);
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
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  int nXStartArc, nYStartArc, nXEndArc, nYEndArc;

  GDK_NOTE (MISC, g_print ("gdk_draw_arc: %#x  %d,%d,%d,%d  %d %d\n",
			   (guint) GDK_DRAWABLE_HANDLE (drawable),
			   x, y, width, height, angle1, angle2));

  /* Seems that drawing arcs with width or height <= 2 fails, at least
   * with my TNT card.
   */
  if (width <= 2 || height <= 2 || angle2 == 0)
    return;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

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
  
  /* GDK_OPAQUE_STIPPLED arcs not implemented. */
  
  if (filled)
    {
      GDK_NOTE (MISC, g_print ("...Pie(hdc,%d,%d,%d,%d,%d,%d,%d,%d)\n",
			       x, y, x+width, y+height,
			       nXStartArc, nYStartArc,
			       nXEndArc, nYEndArc));
      if (!Pie (hdc, x, y, x+width, y+height,
		nXStartArc, nYStartArc, nXEndArc, nYEndArc))
	WIN32_GDI_FAILED ("Pie");
    }
  else
    {
      GDK_NOTE (MISC, g_print ("...Arc(hdc,%d,%d,%d,%d,%d,%d,%d,%d)\n",
			       x, y, x+width, y+height,
			       nXStartArc, nYStartArc,
			       nXEndArc, nYEndArc));
      if (!Arc (hdc, x, y, x+width, y+height,
		nXStartArc, nYStartArc, nXEndArc, nYEndArc))
	WIN32_GDI_FAILED ("Arc");
    }
  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_polygon (GdkDrawable *drawable,
			GdkGC       *gc,
			gint         filled,
			GdkPoint    *points,
			gint         npoints)
{
  GdkGCWin32 *gc_private = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  POINT *pts;
  gboolean ok = TRUE;
  int i;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_polygon: %#x (%p) %d\n",
			   (guint) GDK_DRAWABLE_HANDLE (drawable),
			   gc_private,
			   npoints));

  if (npoints < 2)
    return;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);
  pts = g_new (POINT, npoints+1);

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if (gc_private->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;

      if (ok)
	MoveToEx (hdc, points[0].x, points[0].y, NULL);

      if (pts[0].x == pts[npoints-1].x && pts[0].y == pts[npoints-1].y)
	npoints--;

      if (ok && !Polyline (hdc, pts, 4))
	WIN32_GDI_FAILED ("Polyline"), ok = FALSE;
	  
      if (ok && !CloseFigure (hdc))
	WIN32_GDI_FAILED ("CloseFigure"), ok = FALSE;

      if (ok && !EndPath (hdc))
	WIN32_GDI_FAILED ("EndPath"), ok = FALSE;
	  
      if (ok && !filled)
	if (!WidenPath (hdc))
	  WIN32_GDI_FAILED ("WidenPath"), ok = FALSE;
	  
      if (ok && !FillPath (hdc))
	WIN32_GDI_FAILED ("FillPath"), ok = FALSE;
    }
  else
    {
      if (points[0].x != points[npoints-1].x
	  || points[0].y != points[npoints-1].y) 
	{
	  pts[npoints].x = points[0].x;
	  pts[npoints].y = points[0].y;
	  npoints++;
	}
      
      if (filled)
	{
	  if (!Polygon (hdc, pts, npoints))
	    WIN32_GDI_FAILED ("Polygon");
	}
      else
	{
	  if (!Polyline (hdc, pts, npoints))
	    WIN32_GDI_FAILED ("Polyline");
	}
    }
  g_free (pts);
  gdk_win32_hdc_release (drawable, gc, mask);
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

  if ((oldfont = SelectObject (argp->hdc, singlefont->hfont)) == NULL)
    {
      WIN32_GDI_FAILED ("SelectObject");
      return;
    }
  
  if (!TextOutW (argp->hdc, argp->x, argp->y, wcstr, wclen))
    WIN32_GDI_FAILED ("TextOutW");
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
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_FONT;
  wchar_t *wcstr, wc;
  gint wlen;
  gdk_draw_text_arg arg;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_win32_hdc_get (drawable, gc, mask);

  GDK_NOTE (MISC, g_print ("gdk_draw_text: %#x (%d,%d) \"%.*s\" (len %d)\n",
			   (guint) GDK_DRAWABLE_HANDLE (drawable),
			   x, y,
			   (text_length > 10 ? 10 : text_length),
			   text, text_length));
  
  if (text_length == 1)
    {
      /* For single characters, don't try to interpret as UTF-8. */
      wc = (guchar) text[0];
      gdk_wchar_text_handle (font, &wc, 1, gdk_draw_text_handler, &arg);
    }
  else
    {
      wcstr = g_new (wchar_t, text_length);
      if ((wlen = gdk_nmbstowchar_ts (wcstr, text, text_length, text_length)) == -1)
	g_warning ("gdk_win32_draw_text: gdk_nmbstowchar_ts failed");
      else
	gdk_wchar_text_handle (font, wcstr, wlen, gdk_draw_text_handler, &arg);
      g_free (wcstr);
    }


  gdk_win32_hdc_release (drawable, gc, mask);
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
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_FONT;
  gint i;
  wchar_t *wcstr;
  gdk_draw_text_arg arg;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_win32_hdc_get (drawable, gc, mask);

  GDK_NOTE (MISC, g_print ("gdk_draw_text_wc: %#x (%d,%d) len: %d\n",
			   (guint) GDK_DRAWABLE_HANDLE (drawable),
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

  gdk_win32_hdc_release (drawable, gc, mask);
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
  HDC hdc;
  HDC srcdc;
  HGDIOBJ hgdiobj;
  HRGN src_rgn, draw_rgn, outside_rgn;
  RECT r;
  gint src_width, src_height;
  gboolean ok = TRUE;
  GdkDrawableImplWin32 *impl;
  HANDLE src_handle;

  impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  if (GDK_IS_DRAWABLE_IMPL_WIN32(src))
    src_handle = GDK_DRAWABLE_IMPL_WIN32 (src)->handle;
  else
    src_handle = GDK_DRAWABLE_HANDLE (src);

  GDK_NOTE (MISC, g_print ("gdk_draw_pixmap: dest: %#x @+%d+%d"
			   "src: %#x %dx%d@+%d+%d\n",
		 	   (guint) GDK_DRAWABLE_HANDLE (drawable),
			   xdest, ydest,
			   (guint) GDK_PIXMAP_HBITMAP (src),
			   width, height, xsrc, ysrc));

  hdc = gdk_win32_hdc_get (drawable, gc, 0);

  gdk_drawable_get_size (src, &src_width, &src_height);
  src_rgn = CreateRectRgn (0, 0, src_width + 1, src_height + 1);
  draw_rgn = CreateRectRgn (xsrc, ysrc, xsrc + width + 1, ysrc + height + 1);
  
  if (GDK_IS_WINDOW_IMPL_WIN32 (drawable))
    {
      /* If we are drawing on a window, calculate the region that is
       * outside the source pixmap, and invalidate that, causing it to
       * be cleared. XXX
       */
      SetRectEmpty (&r);
      outside_rgn = CreateRectRgnIndirect (&r);
      if (CombineRgn (outside_rgn, draw_rgn, src_rgn, RGN_DIFF) != NULLREGION)
	{
	  if (ERROR == OffsetRgn (outside_rgn, xdest, ydest))
	    WIN32_GDI_FAILED ("OffsetRgn");
	  GDK_NOTE (MISC, (GetRgnBox (outside_rgn, &r),
			   g_print ("...calling InvalidateRgn, "
				    "bbox: %ldx%ld@+%ld+%ld\n",
				    r.right - r.left - 1, r.bottom - r.top - 1,
				    r.left, r.top)));
	  if (!InvalidateRgn (impl->handle, outside_rgn, TRUE))
	    WIN32_GDI_FAILED ("InvalidateRgn");
	}
      if (!DeleteObject (outside_rgn))
	WIN32_GDI_FAILED ("DeleteObject");
    }

#if 1 /* Don't know if this is necessary  */
  if (CombineRgn (draw_rgn, draw_rgn, src_rgn, RGN_AND) == COMPLEXREGION)
    g_warning ("gdk_win32_draw_drawable: CombineRgn returned a COMPLEXREGION");

  if (0 == GetRgnBox (draw_rgn, &r))
    WIN32_GDI_FAILED("GetRgnBox");
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

  if (!DeleteObject (src_rgn))
    WIN32_GDI_FAILED ("DeleteObject");
  if (!DeleteObject (draw_rgn))
    WIN32_GDI_FAILED ("DeleteObject");

  /* This function is called also to bitblt from a window.
   */
  if (GDK_IS_PIXMAP_IMPL_WIN32 (src) || GDK_IS_PIXMAP(src))
    {
      if ((srcdc = CreateCompatibleDC (hdc)) == NULL)
	WIN32_GDI_FAILED ("CreateCompatibleDC"), ok = FALSE;
      
      if (ok && (hgdiobj = SelectObject (srcdc, src_handle)) == NULL)
	WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;
      
      if (ok && !BitBlt (hdc, xdest, ydest, width, height,
			 srcdc, xsrc, ysrc, SRCCOPY))
	WIN32_GDI_FAILED ("BitBlt");
      
      if (ok && (SelectObject (srcdc, hgdiobj) == NULL))
	WIN32_GDI_FAILED ("SelectObject");
      
      if (srcdc != NULL && !DeleteDC (srcdc))
	WIN32_GDI_FAILED ("DeleteDC");
    }
  else if (impl->handle == src_handle)
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
	WIN32_GDI_FAILED ("ScrollDC"), ok = FALSE;
      if (ok && !InvalidateRgn (impl->handle, updateRgn, FALSE))
	WIN32_GDI_FAILED ("InvalidateRgn"), ok = FALSE;
      if (ok && !UpdateWindow (impl->handle))
	WIN32_GDI_FAILED ("UpdateWindow");
      if (!DeleteObject (updateRgn))
        WIN32_GDI_FAILED ("DeleteObject");
    }
  else
    {
      if ((srcdc = GetDC (src_handle)) == NULL)
	WIN32_GDI_FAILED ("GetDC"), ok = FALSE;
      
      if (ok && !BitBlt (hdc, xdest, ydest, width, height,
			 srcdc, xsrc, ysrc, SRCCOPY))
	WIN32_GDI_FAILED ("BitBlt");
      if (ok && !ReleaseDC (src_handle, srcdc))
	WIN32_GDI_FAILED ("ReleaseDC");
    }
  gdk_win32_hdc_release (drawable, gc, 0);
}

static void
gdk_win32_draw_points (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  HDC hdc;
  COLORREF fg;
  GdkGCWin32 *gc_private = GDK_GC_WIN32 (gc);
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  int i;

  hdc = gdk_win32_hdc_get (drawable, gc, 0);
  
  fg = gdk_colormap_color (impl->colormap, gc_private->foreground);

  GDK_NOTE (MISC, g_print ("gdk_draw_points: %#x %dx%.06x\n",
			   (guint) GDK_DRAWABLE_HANDLE (drawable),
			   npoints, (guint) fg));

  for (i = 0; i < npoints; i++)
    SetPixel (hdc, points[i].x, points[i].y, fg);

  gdk_win32_hdc_release (drawable, gc, 0);
}

static void
gdk_win32_draw_segments (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkSegment  *segs,
			 gint         nsegs)
{
  GdkGCWin32 *gc_private = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  gboolean ok = TRUE;
  int i;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_segments: %#x nsegs: %d\n",
			   (guint) GDK_DRAWABLE_HANDLE (drawable), nsegs));

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  if (gc_private->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;
      
      for (i = 0; ok && i < nsegs; i++)
	{
	  if (ok && !MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
	    WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
	  if (ok && !LineTo (hdc, segs[i].x2, segs[i].y2))
	    WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	  
	  /* Draw end pixel */
	  if (ok && gc_private->pen_width <= 1)
	    if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
	      WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	}

      if (ok && !EndPath (hdc))
	WIN32_GDI_FAILED ("EndPath"), ok = FALSE;
	  
      if (ok && !WidenPath (hdc))
	WIN32_GDI_FAILED ("WidenPath"), ok = FALSE;
	  
      if (ok && !FillPath (hdc))
	WIN32_GDI_FAILED ("FillPath"), ok = FALSE;
    }
  else
    {
      for (i = 0; ok && i < nsegs; i++)
	{
	  if (!MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
	    WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
	  if (ok && !LineTo (hdc, segs[i].x2, segs[i].y2))
	    WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	  
	  /* Draw end pixel */
	  if (ok && gc_private->pen_width <= 1)
	    if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
	      WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	}
    }
  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_lines (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkPoint    *points,
		      gint         npoints)
{
  GdkGCWin32 *gc_private = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  POINT *pts;
  int i;
  gboolean ok = TRUE;

  if (npoints < 2)
    return;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  pts = g_new (POINT, npoints);

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if (!Polyline (hdc, pts, npoints))
    WIN32_GDI_FAILED ("Polyline"), ok = FALSE;
  
  g_free (pts);
  
  /* Draw end pixel */
  if (ok && gc_private->pen_width <= 1)
    {
      MoveToEx (hdc, points[npoints-1].x, points[npoints-1].y, NULL);
      if (!LineTo (hdc, points[npoints-1].x + 1, points[npoints-1].y))
	WIN32_GDI_FAILED ("LineTo");
    }
  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_glyphs (GdkDrawable      *drawable,
		       GdkGC            *gc,
		       PangoFont        *font,
		       gint              x,
		       gint              y,
		       PangoGlyphString *glyphs)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND;
  HDC hdc;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  /* HB: Maybe there should be a GDK_GC_PANGO flag for hdc_get */
  /* default write mode is transparent (leave background) */
  if (SetBkMode (hdc, TRANSPARENT) == 0)
    WIN32_GDI_FAILED ("SetBkMode");

  if (GDI_ERROR == SetTextAlign (hdc, TA_LEFT|TA_BOTTOM|TA_NOUPDATECP))
    WIN32_GDI_FAILED ("SetTextAlign");

  pango_win32_render (hdc, font, glyphs, x, y);

  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_image (GdkDrawable     *drawable,
		      GdkGC           *gc,
		      GdkImage        *image,
		      gint             xsrc,
		      gint             ysrc,
		      gint             xdest,
		      gint             ydest,
		      gint             width,
		      gint             height)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  GdkImagePrivateWin32 *image_private = IMAGE_PRIVATE_DATA (image);
  GdkColormapPrivateWin32 *colormap_private = (GdkColormapPrivateWin32 *) impl->colormap;
  HDC hdc, memdc;
  HGDIOBJ oldbitmap;
  DIBSECTION ds;
  static struct {
    BITMAPINFOHEADER bmiHeader;
    WORD bmiIndices[256];
  } bmi;
  static gboolean bmi_inited = FALSE;
  gboolean ok = TRUE;
  int i;

  hdc = gdk_win32_hdc_get (drawable, gc, 0);

  if (image->visual->type == GDK_VISUAL_PSEUDO_COLOR &&
	  colormap_private && colormap_private->xcolormap->rc_palette)
    {
      if (!bmi_inited)
	{
	  for (i = 0; i < 256; i++)
	    bmi.bmiIndices[i] = i;
	  bmi_inited = TRUE;
	}

      if (GetObject (image_private->hbitmap, sizeof (DIBSECTION),
		     &ds) != sizeof (DIBSECTION))
	WIN32_GDI_FAILED ("GetObject"), ok = FALSE;
#if 0
      g_print("xdest = %d, ydest = %d, xsrc = %d, ysrc = %d, width = %d, height = %d\n",
	      xdest, ydest, xsrc, ysrc, width, height);
      g_print("bmWidth = %d, bmHeight = %d, bmBitsPixel = %d, bmBits = %p\n",
	      ds.dsBm.bmWidth, ds.dsBm.bmHeight, ds.dsBm.bmBitsPixel, ds.dsBm.bmBits);
      g_print("biWidth = %d, biHeight = %d, biBitCount = %d, biClrUsed = %d\n",
	      ds.dsBmih.biWidth, ds.dsBmih.biHeight, ds.dsBmih.biBitCount, ds.dsBmih.biClrUsed);
#endif
      bmi.bmiHeader = ds.dsBmih;
      /* I have spent hours on getting the parameters to
       * SetDIBitsToDevice right. I wonder what drugs the guys in
       * Redmond were on when they designed this API.
       */
      if (ok && SetDIBitsToDevice (hdc,
				   xdest, ydest,
				   width, height,
				   xsrc, (-ds.dsBmih.biHeight)-height-ysrc,
				   0, -ds.dsBmih.biHeight,
				   ds.dsBm.bmBits,
				   (CONST BITMAPINFO *) &bmi,
				   DIB_PAL_COLORS) == 0)
	WIN32_GDI_FAILED ("SetDIBitsToDevice");
    }
  else
    {

      if ((memdc = CreateCompatibleDC (hdc)) == NULL)
	WIN32_GDI_FAILED ("CreateCompatibleDC"), ok = FALSE;

      if (ok && (oldbitmap = SelectObject (memdc, image_private->hbitmap)) == NULL)
	WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;

      if (ok && !BitBlt (hdc, xdest, ydest, width, height,
			 memdc, xsrc, ysrc, SRCCOPY))
	WIN32_GDI_FAILED ("BitBlt");

      if (oldbitmap != NULL && SelectObject (memdc, oldbitmap) == NULL)
	WIN32_GDI_FAILED ("SelectObject");

      if (memdc != NULL && !DeleteDC (memdc))
	WIN32_GDI_FAILED ("DeleteDC");
    }
  gdk_win32_hdc_release (drawable, gc, 0);
}

static gint
gdk_win32_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_WIN32 (drawable)->wrapper);
}

static GdkVisual*
gdk_win32_get_visual (GdkDrawable *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_WIN32 (drawable)->wrapper);
}
