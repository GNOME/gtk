/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include <stdio.h>
#include <glib.h>

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

#ifdef G_ENABLE_DEBUG

gchar *
gdk_win32_drawable_type_to_string (GdkDrawableType type)
{
  static gchar buf[20];

  switch (type)
    {
    case GDK_WINDOW_ROOT: return "ROOT";
    case GDK_WINDOW_TOPLEVEL: return "TOPLEVEL";
    case GDK_WINDOW_CHILD: return "CHILD";
    case GDK_WINDOW_DIALOG: return "DIALOG";
    case GDK_WINDOW_TEMP: return "TEMP";
    case GDK_DRAWABLE_PIXMAP: return "PIXMAP";
    case GDK_WINDOW_FOREIGN: return "FOREIGN";
    default:
      sprintf (buf, "???(%d)", type);
      return buf;
    }
}

gchar *
gdk_win32_drawable_description (GdkDrawable *d)
{
  GdkDrawablePrivate *dp = (GdkDrawablePrivate *) d;
  static gchar buf[1000];
  static gchar *bufp = buf;
  gchar *msg;
  gchar *retval;

  msg = g_strdup_printf
    ("%s:%p:%dx%dx%d",
     gdk_win32_drawable_type_to_string (GDK_DRAWABLE_TYPE (d)),
     GDK_DRAWABLE_XID (d),
     dp->width, dp->height,
     (GDK_IS_PIXMAP (d) ? GDK_DRAWABLE_WIN32DATA (d)->image->depth
      : ((GdkColormapPrivate *) dp->colormap)->visual->depth));

  if (bufp + strlen (msg) + 1 > buf + sizeof (buf))
    bufp = buf;
  retval = bufp;
  strcpy (bufp, msg);
  bufp += strlen (msg) + 1;
  g_free (msg);

  return retval;
}

#endif

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
      if (drawable_private->colormap == NULL &&
	  GDK_IS_WINDOW (drawable))
	drawable_private->colormap = gdk_colormap_get_system ();
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
	  g_return_if_fail (colormap_private->base.visual !=
			    ((GdkColormapPrivate *) (drawable_private->colormap))->visual);
	  /* XXX ??? */
	  GDK_NOTE (MISC, g_print ("gdk_drawable_set_colormap: %p %p\n",
				   GDK_DRAWABLE_XID (drawable),
				   colormap_private->hpal));
	}
      if (drawable_private->colormap)
	gdk_colormap_unref (drawable_private->colormap);
      drawable_private->colormap = colormap;
      gdk_colormap_ref (drawable_private->colormap);
      
      if (GDK_IS_WINDOW (drawable)
	  && drawable_private->window_type != GDK_WINDOW_TOPLEVEL)
	gdk_window_add_colormap_windows (drawable);
    }
}

/* Drawing
 */
static void 
gdk_win32_drawable_destroy (GdkDrawable *drawable)
{
  g_assert_not_reached ();
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
  GdkGCPrivate *gc_private = (GdkGCPrivate*) gc;
  GdkGCWin32Data *gc_data = GDK_GC_WIN32DATA (gc_private);
  HDC hdc;
  HGDIOBJ oldpen_or_brush;
  POINT pts[4];
  gboolean ok = TRUE;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_rectangle: %p (%p) %s%dx%d@+%d+%d\n",
			   GDK_DRAWABLE_XID (drawable),
			   gc_private,
			   (filled ? "fill " : ""),
			   width, height, x, y));
    
  hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);

  if (gc_data->fill_style == GDK_OPAQUE_STIPPLED)
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
#if 0
  else if (gc_data->fill_style == GDK_STIPPLED)
    {
      HDC memdc = CreateCompatibleDC (hdc);
      HBITMAP hbm = CreateCompatibleBitmap (hdc, width, height);
      HBRUSH hbr = CreateSolidBrush (gc_data->....);
      HBRUSH stipple = CreatePatternBrush (GDK_DRAWABLE_XID (gc_data->stipple));
      SetBrushOrgEx (hdc, gc_data->ts_x_origin, gc_data->ts_y_origin);
      oldpen_or_brush = Select

      SelectObject (memdc, hbm);

      rect.left = rect.top = 0;
      rect.right = width;
      rect.bottom = height;

      FillRect (memdc, &rect, hbr);
      
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
#endif
  else
    {
      if (filled)
	oldpen_or_brush = SelectObject (hdc, GetStockObject (NULL_PEN));
      else
	oldpen_or_brush = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));

      if (oldpen_or_brush == NULL)
	WIN32_GDI_FAILED ("SelectObject");
      
      if (!Rectangle (hdc, x, y, x+width+1, y+height+1))
	WIN32_GDI_FAILED ("Rectangle");
  
      if (oldpen_or_brush != NULL &&
	  SelectObject (hdc, oldpen_or_brush) == NULL)
	WIN32_GDI_FAILED ("SelectObject");
    }

  gdk_gc_postdraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);
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

  GDK_NOTE (MISC, g_print ("gdk_draw_arc: %p  %d,%d,%d,%d  %d %d\n",
			   GDK_DRAWABLE_XID (drawable),
			   x, y, width, height, angle1, angle2));

  /* Seems that drawing arcs with width or height <= 2 fails, at least
   * with my TNT card.
   */
  if (width <= 2 || height <= 2 || angle2 == 0)
    return;

  hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);

  if (angle2 >= 360*64)
    {
      nXStartArc = nYStartArc = nXEndArc = nYEndArc = 0;
    }
  else if (angle2 > 0)
    {
      /* The 100. is just an arbitrary value */
      nXStartArc = x + width/2 + 100. * cos (angle1/64.*2.*G_PI/360.);
      nYStartArc = y + height/2 + -100. * sin (angle1/64.*2.*G_PI/360.);
      nXEndArc = x + width/2 + 100. * cos ((angle1+angle2)/64.*2.*G_PI/360.);
      nYEndArc = y + height/2 + -100. * sin ((angle1+angle2)/64.*2.*G_PI/360.);
    }
  else
    {
      nXEndArc = x + width/2 + 100. * cos (angle1/64.*2.*G_PI/360.);
      nYEndArc = y + height/2 + -100. * sin (angle1/64.*2.*G_PI/360.);
      nXStartArc = x + width/2 + 100. * cos ((angle1+angle2)/64.*2.*G_PI/360.);
      nYStartArc = y + height/2 + -100. * sin ((angle1+angle2)/64.*2.*G_PI/360.);
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
  gdk_gc_postdraw (drawable, gc_private,
		   GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);
}

static void
gdk_win32_draw_polygon (GdkDrawable *drawable,
			GdkGC       *gc,
			gint         filled,
			GdkPoint    *points,
			gint         npoints)
{
  GdkGCPrivate *gc_private = (GdkGCPrivate*) gc;
  GdkGCWin32Data *gc_data = GDK_GC_WIN32DATA (gc_private);
  HDC hdc;
  POINT *pts;
  gboolean ok = TRUE;
  int i;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_polygon: %p (%p) %d\n",
			   GDK_DRAWABLE_XID (drawable), gc_private,
			   npoints));

  if (npoints < 2)
    return;

  hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);
  pts = g_new (POINT, npoints+1);

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if (gc_data->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;

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
  gdk_gc_postdraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);
}

typedef struct
{
  gint x, y;
  HDC hdc;
  int total_len;
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

  oldfont = GetCurrentObject(argp->hdc, OBJ_FONT);
  if (oldfont != singlefont->xfont) 
    {
      if (SelectObject (argp->hdc, singlefont->xfont) == NULL)
        {
          WIN32_GDI_FAILED ("SelectObject");
          return;
        }
    }
  
  if (!TextOutW (argp->hdc, argp->x, argp->y, wcstr, wclen))
    WIN32_GDI_FAILED ("TextOutW");

  /* Find width of string iff printing part of
   * text. GetTextExtentPoint32W() is slow.
   */
  if (wclen < argp->total_len)
     {
       if (!GetTextExtentPoint32W (argp->hdc, wcstr, wclen, &size))
         WIN32_GDI_FAILED  ("GetTextExtentPoint32W");
       else
	 argp->x += size.cx;
     }
     
  if (oldfont != singlefont->xfont) 
    {
      if (SelectObject (argp->hdc, oldfont) == NULL)
	WIN32_GDI_FAILED ("SelectObject");
    }
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

  arg.hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_FONT);

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_text: %p (%d,%d) \"%.*s\" (len %d)\n",
			   GDK_DRAWABLE_XID (drawable),
			   x, y,
			   (text_length > 10 ? 10 : text_length),
			   text, text_length));
  
  wcstr = g_new (wchar_t, text_length);
  if ((wlen = gdk_nmbstowchar_ts (wcstr, text, text_length, text_length)) == -1)
    g_warning ("gdk_win32_draw_text: gdk_nmbstowchar_ts failed");
  else
    {
      arg.total_len = wlen;
      gdk_wchar_text_handle (font, wcstr, wlen, gdk_draw_text_handler, &arg);
    }

  g_free (wcstr);

  gdk_gc_postdraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_FONT);
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
  gint i;
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

  arg.hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_FONT);

  GDK_NOTE (MISC, g_print ("gdk_draw_text_wc: %p (%d,%d) len: %d\n",
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

  arg.total_len = text_length;
  gdk_wchar_text_handle (font, wcstr, text_length, gdk_draw_text_handler, &arg);

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    g_free (wcstr);

  gdk_gc_postdraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_FONT);
}

static void
blit_from_pixmap (gboolean        use_fg_bg,
		  GdkDrawable    *dst,
		  HDC             hdc,
		  GdkDrawable    *src,
		  GdkGCWin32Data *gcwin32,
		  gint            xsrc,
		  gint            ysrc,
		  gint         	  xdest,
		  gint         	  ydest,
		  gint         	  width,
		  gint         	  height)
{
  HDC srcdc;
  HBITMAP holdbitmap;
  RGBQUAD oldtable[256], newtable[256];
  COLORREF bg, fg;
  GdkColormapPrivateWin32 *cmapp;
  GdkDrawablePrivate *dst_private = (GdkDrawablePrivate *) dst;

  gint newtable_size = 0, oldtable_size = 0;
  gboolean ok = TRUE;
  
  GDK_NOTE (MISC, g_print ("blit_from_pixmap\n"));

  if (!(srcdc = CreateCompatibleDC (NULL)))
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      return;
    }
      
  gdk_win32_clear_hdc_cache_for_hwnd (GDK_DRAWABLE_XID (src));

  if (!(holdbitmap = SelectObject (srcdc, GDK_DRAWABLE_XID (src))))
    WIN32_GDI_FAILED ("SelectObject");
  else
    {
      if (GDK_DRAWABLE_WIN32DATA (src)->image->depth <= 8)
	{
	  /* Blitting from a 1, 4 or 8-bit pixmap */

	  if ((oldtable_size = GetDIBColorTable (srcdc, 0, 256, oldtable)) == 0)
	    WIN32_GDI_FAILED ("GetDIBColorTable");
	  else if (GDK_DRAWABLE_WIN32DATA (src)->image->depth == 1)
	    {
	      /* Blitting from an 1-bit pixmap */

	      gint bgix, fgix;
	      
	      if (use_fg_bg)
		{
		  bgix = gcwin32->background;
		  fgix = gcwin32->foreground;
		}
	      else
		{
		  bgix = 0;
		  fgix = 1;
		}

	      if (GDK_IS_PIXMAP (dst) &&
		  GDK_DRAWABLE_WIN32DATA (dst)->image->depth <= 8)
		{
		  /* Destination is also pixmap, get fg and bg from
		   * its palette. Either use the foreground and
		   * background pixel values in the GC (only in the
		   * case of gdk_image_put(), cf. XPutImage()), or 0
		   * and 1 to index the palette.
		   */
		  if (!GetDIBColorTable (hdc, bgix, 1, newtable) ||
		      !GetDIBColorTable (hdc, fgix, 1, newtable+1))
		    WIN32_GDI_FAILED ("GetDIBColorTable"), ok = FALSE;
		}
	      else
		{
		  /* Destination is a window, get fg and bg from its
		   * colormap
		   */

		  cmapp = (GdkColormapPrivateWin32 *) dst_private->colormap;
		  
		  bg = gdk_win32_colormap_color_pack (cmapp, bgix);
		  fg = gdk_win32_colormap_color_pack (cmapp, fgix);
		  newtable[0].rgbBlue = GetBValue (bg);
		  newtable[0].rgbGreen = GetGValue (bg);
		  newtable[0].rgbRed = GetRValue (bg);
		  newtable[0].rgbReserved = 0;
		  newtable[1].rgbBlue = GetBValue (fg);
		  newtable[1].rgbGreen = GetGValue (fg);
		  newtable[1].rgbRed = GetRValue (fg);
		  newtable[1].rgbReserved = 0;
		}
	      if (ok)
		GDK_NOTE (MISC, g_print ("bg: %02x %02x %02x "
					 "fg: %02x %02x %02x\n",
					 newtable[0].rgbRed,
					 newtable[0].rgbGreen,
					 newtable[0].rgbBlue,
					 newtable[1].rgbRed,
					 newtable[1].rgbGreen,
					 newtable[1].rgbBlue));
	      newtable_size = 2;
	    }
	  else if (GDK_IS_PIXMAP (dst))
	    {
	      /* Destination is pixmap, get its color table */
	      
	      if ((newtable_size = GetDIBColorTable (hdc, 0, 256, newtable)) == 0)
		WIN32_GDI_FAILED ("GetDIBColorTable"), ok = FALSE;
	    }
	  
	  /* If blitting between pixmaps, set source's color table */
	  if (ok && newtable_size > 0)
	    {
	      GDK_NOTE (COLORMAP, g_print ("blit_from_pixmap: set color table"
					   " hdc=%p count=%d\n",
					   srcdc, newtable_size));
	      if (!SetDIBColorTable (srcdc, 0, newtable_size, newtable))
		WIN32_GDI_FAILED ("SetDIBColorTable"), ok = FALSE;
	    }
	}
      
      if (ok && !BitBlt (hdc, xdest, ydest, width, height,
			 srcdc, xsrc, ysrc, SRCCOPY))
	WIN32_GDI_FAILED ("BitBlt");
      
      /* Restore source's color table if necessary */
      if (ok && oldtable_size > 0)
	{
	  GDK_NOTE (COLORMAP, g_print ("blit_from_pixmap: reset color table"
				       " hdc=%p count=%d\n",
				       srcdc, oldtable_size));
	  if (!SetDIBColorTable (srcdc, 0, oldtable_size, oldtable))
	    WIN32_GDI_FAILED ("SetDIBColorTable");
	}
      
      if (!SelectObject (srcdc, holdbitmap))
	WIN32_GDI_FAILED ("SelectObject");
    }
  if (!DeleteDC (srcdc))
    WIN32_GDI_FAILED ("DeleteDC");
}

static void
blit_inside_window (GdkDrawable *window,
		    HDC          hdc,
		    gint         xsrc,
		    gint         ysrc,
		    gint         xdest,
		    gint         ydest,
		    gint         width,
		    gint         height)

{
  RECT scrollRect, clipRect, emptyRect;
  HRGN updateRgn;
      
  GDK_NOTE (MISC, g_print ("blit_inside_window\n"));

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
    WIN32_GDI_FAILED ("ScrollDC");
  else if (!InvalidateRgn (GDK_DRAWABLE_XID (window), updateRgn, FALSE))
    WIN32_GDI_FAILED ("InvalidateRgn");
  else if (GDK_WINDOW_WIN32DATA (window)->owner_thread_id != GetCurrentThreadId ())
    {
      if (!PostThreadMessage (GDK_WINDOW_WIN32DATA (window)->owner_thread_id, WM_PAINT, 0, 0))
	WIN32_API_FAILED ("PostThreadMessage");
    }
  else if (!UpdateWindow (GDK_DRAWABLE_XID (window)))
    WIN32_GDI_FAILED ("UpdateWindow");

  if (!DeleteObject (updateRgn))
    WIN32_GDI_FAILED ("DeleteObject");
}

static void
blit_from_window (HDC          hdc,
		  GdkDrawable *src,
		  gint         xsrc,
		  gint         ysrc,
		  gint         xdest,
		  gint         ydest,
		  gint         width,
		  gint         height)
{
  HDC srcdc;
  HPALETTE holdpal = NULL;
  GdkColormapPrivateWin32 *cmapp;

  GDK_NOTE (MISC, g_print ("blit_from_window\n"));

  if ((srcdc = GetDC (GDK_DRAWABLE_XID (src))) == NULL)
    {
      WIN32_GDI_FAILED ("GetDC");
      return;
    }

  cmapp = (GdkColormapPrivateWin32 *) gdk_colormap_get_system ();

  if (cmapp->base.visual->type == GDK_VISUAL_PSEUDO_COLOR ||
      cmapp->base.visual->type == GDK_VISUAL_STATIC_COLOR)
    {
      gint k;
      
      if (!(holdpal = SelectPalette (srcdc, cmapp->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (srcdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (COLORMAP,
		  g_print ("blit_from_window: realized %d\n", k));
    }
  
  if (!BitBlt (hdc, xdest, ydest, width, height,
	       srcdc, xsrc, ysrc, SRCCOPY))
    WIN32_GDI_FAILED ("BitBlt");
  
  if (holdpal != NULL)
    SelectPalette (srcdc, holdpal, FALSE);
  
  if (!ReleaseDC (GDK_DRAWABLE_XID (src), srcdc))
    WIN32_GDI_FAILED ("ReleaseDC");
}

void
gdk_win32_blit (gboolean    use_fg_bg,
		GdkDrawable *drawable,
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
  HRGN src_rgn, draw_rgn, outside_rgn;
  RECT r;
  GdkDrawablePrivate *src_private = (GdkDrawablePrivate*) src;
  
  GDK_NOTE (MISC, g_print ("gdk_win32_blit: src:%s %dx%d@+%d+%d\n"
			   "                dst:%s @+%d+%d use_fg_bg=%d\n",
			   gdk_win32_drawable_description (src),
			   width, height, xsrc, ysrc,
			   gdk_win32_drawable_description (drawable),
			   xdest, ydest,
			   use_fg_bg));

  hdc = gdk_gc_predraw (drawable, (GdkGCPrivate *) gc, GDK_GC_FOREGROUND);

  if ((src_rgn = CreateRectRgn (0, 0,
				src_private->width + 1,
				src_private->height + 1)) == NULL)
    WIN32_GDI_FAILED ("CreateRectRgn");
  else if ((draw_rgn = CreateRectRgn (xsrc, ysrc,
				      xsrc + width + 1,
				      ysrc + height + 1)) == NULL)
    WIN32_GDI_FAILED ("CreateRectRgn");
  else
    {
      if (!GDK_IS_PIXMAP (drawable))
	{
	  int comb;
	  
	  /* If we are drawing on a window, calculate the region that is
	   * outside the source pixmap, and invalidate that, causing it to
	   * be cleared. Not completely sure whether this is always needed. XXX
	   */
	  SetRectEmpty (&r);
	  outside_rgn = CreateRectRgnIndirect (&r);
	  
	  if ((comb = CombineRgn (outside_rgn,
				  draw_rgn, src_rgn,
				  RGN_DIFF)) == ERROR)
	    WIN32_GDI_FAILED ("CombineRgn");
	  else if (comb != NULLREGION)
	    {
	      OffsetRgn (outside_rgn, xdest, ydest);
	      GDK_NOTE (MISC, (GetRgnBox (outside_rgn, &r),
			       g_print ("...calling InvalidateRgn, "
					"bbox: %ldx%ld@+%ld+%ld\n",
					r.right - r.left - 1, r.bottom - r.top - 1,
					r.left, r.top)));
	      InvalidateRgn (GDK_DRAWABLE_XID (drawable), outside_rgn, TRUE);
	    }
	  DeleteObject (outside_rgn);
	}

#if 1 /* Don't know if this is necessary XXX */
      if (CombineRgn (draw_rgn, draw_rgn, src_rgn, RGN_AND) == COMPLEXREGION)
	g_warning ("gdk_win32_blit: CombineRgn returned a COMPLEXREGION");
      
      GetRgnBox (draw_rgn, &r);
      if (r.left != xsrc || r.top != ysrc ||
	  r.right != xsrc + width + 1 || r.bottom != ysrc + height + 1)
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
    }

  if (GDK_IS_PIXMAP (src))
    blit_from_pixmap (use_fg_bg, drawable, hdc, src, GDK_GC_WIN32DATA (gc),
		      xsrc, ysrc, xdest, ydest, width, height);
  else if (GDK_IS_WINDOW (drawable) &&
	   GDK_DRAWABLE_XID (drawable) == GDK_DRAWABLE_XID (src))
    blit_inside_window (drawable, hdc, xsrc, ysrc, xdest, ydest, width, height);
  else
    blit_from_window (hdc, src, xsrc, ysrc, xdest, ydest, width, height);
  gdk_gc_postdraw (drawable, (GdkGCPrivate *) gc, GDK_GC_FOREGROUND);
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
  gdk_win32_blit (FALSE, drawable, gc, src, xsrc, ysrc,
		  xdest, ydest, width, height);
}

static void
gdk_win32_draw_points (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  HDC hdc;
  COLORREF fg;
  GdkGCPrivate *gc_private = (GdkGCPrivate*) gc;
  GdkGCWin32Data *gc_data = GDK_GC_WIN32DATA (gc_private);
  GdkDrawablePrivate *drawable_private = (GdkDrawablePrivate *) drawable;
  GdkColormapPrivateWin32 *colormap_private =
    (GdkColormapPrivateWin32 *) drawable_private->colormap;
  int i;

  hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND);
  
  fg = gdk_win32_colormap_color_pack (colormap_private, gc_data->foreground);

  GDK_NOTE (MISC, g_print ("gdk_draw_points: %p %dx%06x\n",
			   GDK_DRAWABLE_XID (drawable), npoints, (guint) fg));

  for (i = 0; i < npoints; i++)
    SetPixel (hdc, points[i].x, points[i].y, fg);

  gdk_gc_postdraw (drawable, gc_private, GDK_GC_FOREGROUND);
}

static void
gdk_win32_draw_segments (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkSegment  *segs,
			 gint         nsegs)
{
  GdkGCPrivate *gc_private = (GdkGCPrivate*) gc;
  GdkGCWin32Data *gc_data = GDK_GC_WIN32DATA (gc_private);
  HDC hdc;
  gboolean ok = TRUE;
  int i;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_segments: %p nsegs: %d\n",
			   GDK_DRAWABLE_XID (drawable), nsegs));

  hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);

  if (gc_data->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;
      
      for (i = 0; i < nsegs; i++)
	{
	  if (ok && !MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
	    WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
	  if (ok && !LineTo (hdc, segs[i].x2, segs[i].y2))
	    WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	  
	  /* Draw end pixel */
	  if (ok && gc_data->pen_width <= 1)
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
      const gboolean maybe_patblt =
	gc_data->rop2 == R2_COPYPEN &&
	gc_data->pen_width <= 1 &&
	(gc_data->pen_style & PS_STYLE_MASK) == PS_SOLID;

      for (i = 0; ok && i < nsegs; i++)
	{
	  /* PatBlt() is much faster than LineTo(), says
           * jpe@archaeopteryx.com. Hmm. Use it if we have a solid
           * colour pen, then we know that the brush is also solid and
           * of the same colour.
	   */
	  if (maybe_patblt && segs[i].x1 == segs[i].x2)
            {
	      int y1, y2;

	      if (segs[i].y1 <= segs[i].y2)
		y1 = segs[i].y1, y2 = segs[i].y2;
	      else
		y1 = segs[i].y2, y2 = segs[i].y1;

              if (!PatBlt (hdc, segs[i].x1, y1,
			   1, y2 - y1 + 1, PATCOPY))
                WIN32_GDI_FAILED ("PatBlt"), ok = FALSE;
            }
	  else if (maybe_patblt && segs[i].y1 == segs[i].y2)
            {
	      int x1, x2;

	      if (segs[i].x1 <= segs[i].x2)
		x1 = segs[i].x1, x2 = segs[i].x2;
	      else
		x1 = segs[i].x2, x2 = segs[i].x1;

              if (!PatBlt (hdc, x1, segs[i].y1,
			   x2 - x1 + 1, 1, PATCOPY))
                WIN32_GDI_FAILED ("PatBlt"), ok = FALSE;
            }
          else
            {
	      if (!MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
		WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
	      if (!LineTo (hdc, segs[i].x2, segs[i].y2))
		WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	      
	      /* Draw end pixel */
	      if (!IS_WIN_NT (windows_version) && gc_data->pen_width <= 1)
		if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
		  WIN32_GDI_FAILED ("LineTo");
	    }
	}
    }
  gdk_gc_postdraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);
}

static void
gdk_win32_draw_lines (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkPoint    *points,
		      gint         npoints)
{
  GdkGCPrivate *gc_private = (GdkGCPrivate*) gc;
  GdkGCWin32Data *gc_data = GDK_GC_WIN32DATA (gc_private);
  HDC hdc;
  POINT *pts;
  int i;

  if (npoints < 2)
    return;

  hdc = gdk_gc_predraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);
#if 1
  pts = g_new (POINT, npoints);

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if (!Polyline (hdc, pts, npoints))
    WIN32_GDI_FAILED ("Polyline");
  
  g_free (pts);
  
  /* Draw end pixel */
  if (!IS_WIN_NT (windows_version) && gc_data->pen_width <= 1)
    {
      MoveToEx (hdc, points[npoints-1].x, points[npoints-1].y, NULL);
      if (!LineTo (hdc, points[npoints-1].x + 1, points[npoints-1].y))
	WIN32_GDI_FAILED ("LineTo");
    }
#else
  MoveToEx (hdc, points[0].x, points[0].y, NULL);
  for (i = 1; i < npoints; i++)
    if (!LineTo (hdc, points[i].x, points[i].y))
      WIN32_GDI_FAILED ("LineTo");
  
  /* Draw end pixel */
  /* LineTo doesn't draw the last point, so if we have a pen width of 1,
   * we draw the end pixel separately... With wider pens we don't care.
   * //HB: But the NT developers don't read their API documentation ...
   */
  if (gc_data->pen_width <= 1 && !IS_WIN_NT (windows_version))
    if (!LineTo (hdc, points[npoints-1].x + 1, points[npoints-1].y))
      WIN32_GDI_FAILED ("LineTo");
#endif	
  gdk_gc_postdraw (drawable, gc_private, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND);
}

void
gdk_win32_print_dc_attributes (HDC hdc)
{
  HBRUSH hbr = GetCurrentObject (hdc, OBJ_BRUSH);
  HPEN hpen = GetCurrentObject (hdc, OBJ_PEN);
  LOGBRUSH lbr;
  LOGPEN lpen;

  GetObject (hbr, sizeof (lbr), &lbr);
  GetObject (hpen, sizeof (lpen), &lpen);

  g_print ("current brush: style = %s, color = 0x%08lx\n",
	   (lbr.lbStyle == BS_SOLID ? "SOLID" : "???"),
	   lbr.lbColor);
  g_print ("current pen: style = %s, width = %ld, color = 0x%08lx\n",
	   (lpen.lopnStyle == PS_SOLID ? "SOLID" : "???"),
	   lpen.lopnWidth.x,
	   lpen.lopnColor);
}

