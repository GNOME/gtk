/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include <stdio.h>
#include <glib.h>

#include <pango/pangowin32.h>

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

static void gdk_drawable_impl_win32_finalize   (GObject *object);

static gpointer parent_class = NULL;

#ifdef G_ENABLE_DEBUG

gchar *
gdk_win32_drawable_description (GdkDrawable *d)
{
  GdkVisual *v;
  static gchar buf[1000];
  static gchar *bufp = buf;
  gchar *msg;
  gint width, height;
  gchar *retval;

  gdk_drawable_get_size (d, &width, &height);
  msg = g_strdup_printf
    ("%s:%p:%dx%dx%d",
     G_OBJECT_TYPE_NAME (d),
     GDK_DRAWABLE_HANDLE (d),
     width, height,
     (GDK_IS_PIXMAP (d) ? GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (d)->impl)->image->depth
      : ((v = gdk_drawable_get_visual (d)) ? v->depth : gdk_visual_get_system ()->depth)));

  if (bufp + strlen (msg) + 1 > buf + sizeof (buf))
    bufp = buf;
  retval = bufp;
  strcpy (bufp, msg);
  bufp += strlen (msg) + 1;
  g_free (msg);

  return retval;
}

#endif

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
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drawable_impl_win32_finalize;

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

  drawable_class->_copy_to_image = _gdk_win32_copy_to_image;
}

static void
gdk_drawable_impl_win32_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*****************************************************
 * Win32 specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_win32_get_colormap (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_IMPL_WIN32 (drawable)->colormap;
}

static void
gdk_win32_set_colormap (GdkDrawable *drawable,
			GdkColormap *colormap)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

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

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_rectangle: %s (%p) %s%dx%d@+%d+%d\n",
			   gdk_win32_drawable_description (drawable),
			   gc_private,
			   (filled ? "fill " : ""),
			   width, height, x, y));
    
  if (filled 
      && (gc_private->tile)
      && (gc_private->values_mask & GDK_GC_TILE)    
      && (gc_private->values_mask & GDK_GC_FILL))
    {
      _gdk_win32_draw_tiles (drawable, gc, gc_private->tile, x, y, width, height);
      return;
    }

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
      if (old_pen_or_brush == NULL)
	WIN32_GDI_FAILED ("SelectObject");
      else if (!Rectangle (hdc, x, y, x+width+1, y+height+1))
	WIN32_GDI_FAILED ("Rectangle");
      else
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

  GDK_NOTE (MISC, g_print ("gdk_draw_arc: %s  %d,%d,%d,%d  %d %d\n",
			   gdk_win32_drawable_description (drawable),
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

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_polygon: %s (%p) %d\n",
			   gdk_win32_drawable_description (drawable),
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

  GDK_NOTE (MISC, g_print ("gdk_draw_text: %s (%d,%d) \"%.*s\" (len %d)\n",
			   gdk_win32_drawable_description (drawable),
			   x, y,
			   (text_length > 10 ? 10 : text_length),
			   text, text_length));
  
  if (text_length == 1)
    {
      /* For single characters, don't try to interpret as UTF-8. */
      wc = (guchar) text[0];
      _gdk_wchar_text_handle (font, &wc, 1, gdk_draw_text_handler, &arg);
    }
  else
    {
      wcstr = g_new (wchar_t, text_length);
      if ((wlen = _gdk_utf8_to_ucs2 (wcstr, text, text_length, text_length)) == -1)
	g_warning ("gdk_win32_draw_text: _gdk_utf8_to_ucs2 failed");
      else
	_gdk_wchar_text_handle (font, wcstr, wlen, gdk_draw_text_handler, &arg);
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

  GDK_NOTE (MISC, g_print ("gdk_draw_text_wc: %s (%d,%d) len: %d\n",
			   gdk_win32_drawable_description (drawable),
			   x, y, text_length));
      
  if (sizeof (wchar_t) != sizeof (GdkWChar))
    {
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
    }
  else
    wcstr = (wchar_t *) text;

  _gdk_wchar_text_handle (font, wcstr, text_length,
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
  g_assert (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable));

  _gdk_win32_blit (FALSE, (GdkDrawableImplWin32 *) drawable,
		   gc, src, xsrc, ysrc,
		   xdest, ydest, width, height);
}

void
_gdk_win32_draw_tiles (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPixmap   *tile,
		       gint        x_from,
		       gint        y_from,
		       gint        max_width,
		       gint        max_height)
{
  gint x = x_from, y = y_from;
  gint tile_width, tile_height;
  gint width, height;

  GDK_NOTE (MISC, g_print ("_gdk_win32_draw_tiles: %s tile=%s +%d+%d %d,%d\n",
			   gdk_win32_drawable_description (drawable),
			   gdk_win32_drawable_description (tile),
			   x_from, y_from, max_width, max_height));

  gdk_drawable_get_size (drawable, &width, &height);
  gdk_drawable_get_size (tile, &tile_width, &tile_height);

  width  = MIN (width,  max_width);
  height = MIN (height, max_height);

  tile_width  = MIN (tile_width,  max_width);
  tile_height = MIN (tile_height, max_height);

  while (y < height)
    {
      x = x_from;
      while (x < width)
        {
	  gdk_draw_drawable (drawable, gc, tile,
			     x % tile_width,  /* xsrc */
			     y % tile_height, /* ysrc */
			     x, y, /* dest */
			     tile_width, tile_height);
	  x += tile_width;
	}
      y += tile_height;
    }
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
  
  fg = _gdk_win32_colormap_color (impl->colormap, gc_private->foreground);

  GDK_NOTE (MISC, g_print ("gdk_draw_points: %s %dx%.06x\n",
			   gdk_win32_drawable_description (drawable),
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

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_segments: %s nsegs: %d\n",
			   gdk_win32_drawable_description (drawable), nsegs));

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
      const gboolean maybe_patblt =
	gc_private->rop2 == R2_COPYPEN &&
	gc_private->pen_width <= 1 &&
	(gc_private->pen_style & PS_STYLE_MASK) == PS_SOLID;

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
	      if (ok && !LineTo (hdc, segs[i].x2, segs[i].y2))
	        WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	  
	      /* Draw end pixel */
	      if (ok && gc_private->pen_width <= 1)
	        if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
	          WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
            }
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

  if (GDI_ERROR == SetTextAlign (hdc, TA_LEFT|TA_BASELINE|TA_NOUPDATECP))
    WIN32_GDI_FAILED ("SetTextAlign");

  pango_win32_render (hdc, font, glyphs, x, y);

  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
blit_from_pixmap (gboolean              use_fg_bg,
		  GdkDrawableImplWin32 *dest,
		  HDC                   hdc,
		  GdkPixmapImplWin32   *src,
		  GdkGCWin32           *gcwin32,
		  gint         	      	xsrc,
		  gint         	      	ysrc,
		  gint         	      	xdest,
		  gint         	      	ydest,
		  gint         	      	width,
		  gint         	      	height)
{
  HDC srcdc;
  HBITMAP holdbitmap;
  RGBQUAD oldtable[256], newtable[256];
  COLORREF bg, fg;

  gint newtable_size = 0, oldtable_size = 0;
  gboolean ok = TRUE;
  
  GDK_NOTE (MISC, g_print ("blit_from_pixmap\n"));

  if (!(srcdc = CreateCompatibleDC (NULL)))
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      return;
    }
      
  if (!(holdbitmap = SelectObject (srcdc, ((GdkDrawableImplWin32 *) src)->handle)))
    WIN32_GDI_FAILED ("SelectObject");
  else
    {
      if (src->image->depth <= 8)
	{
	  /* Blitting from a 1, 4 or 8-bit pixmap */

	  if ((oldtable_size = GetDIBColorTable (srcdc, 0, 256, oldtable)) == 0)
	    WIN32_GDI_FAILED ("GetDIBColorTable");
	  else if (src->image->depth == 1)
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

	      if (GDK_IS_PIXMAP_IMPL_WIN32 (dest) &&
		  ((GdkPixmapImplWin32 *) dest)->image->depth <= 8)
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

		  bg = _gdk_win32_colormap_color (dest->colormap, bgix);
		  fg = _gdk_win32_colormap_color (dest->colormap, fgix);
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
	  else if (GDK_IS_PIXMAP_IMPL_WIN32 (dest))
	    {
	      /* Destination is pixmap, get its color table */
	      
	      if ((newtable_size = GetDIBColorTable (hdc, 0, 256, newtable)) == 0)
		WIN32_GDI_FAILED ("GetDIBColorTable"), ok = FALSE;
	    }
	  
	  /* If blitting between pixmaps, set source's color table */
	  if (ok && newtable_size > 0)
	    {
	      GDK_NOTE (MISC_OR_COLORMAP,
			g_print ("blit_from_pixmap: set color table"
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
      if (ok && newtable_size > 0 && oldtable_size > 0)
	{
	  GDK_NOTE (MISC_OR_COLORMAP,
		    g_print ("blit_from_pixmap: reset color table"
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
blit_inside_window (GdkDrawableImplWin32 *window,
		    HDC      		  hdc,
		    gint     		  xsrc,
		    gint     		  ysrc,
		    gint     		  xdest,
		    gint     		  ydest,
		    gint     		  width,
		    gint     		  height)

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
  else if (!InvalidateRgn (window->handle, updateRgn, FALSE))
    WIN32_GDI_FAILED ("InvalidateRgn");
  else if (!UpdateWindow (window->handle))
    WIN32_GDI_FAILED ("UpdateWindow");

  if (!DeleteObject (updateRgn))
    WIN32_GDI_FAILED ("DeleteObject");
}

static void
blit_from_window (HDC                   hdc,
		  GdkDrawableImplWin32 *src,
		  gint         	      	xsrc,
		  gint         	      	ysrc,
		  gint         	      	xdest,
		  gint         	      	ydest,
		  gint         	      	width,
		  gint         	      	height)
{
  HDC srcdc;
  HPALETTE holdpal = NULL;
  GdkColormap *cmap = gdk_colormap_get_system ();

  GDK_NOTE (MISC, g_print ("blit_from_window\n"));

  if ((srcdc = GetDC (src->handle)) == NULL)
    {
      WIN32_GDI_FAILED ("GetDC");
      return;
    }

  if (cmap->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
      cmap->visual->type == GDK_VISUAL_STATIC_COLOR)
    {
      gint k;
      
      if (!(holdpal = SelectPalette (srcdc, GDK_WIN32_COLORMAP_DATA (cmap)->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (srcdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (MISC_OR_COLORMAP,
		  g_print ("blit_from_window: realized %d\n", k));
    }
  
  if (!BitBlt (hdc, xdest, ydest, width, height,
	       srcdc, xsrc, ysrc, SRCCOPY))
    WIN32_GDI_FAILED ("BitBlt");
  
  if (holdpal != NULL)
    SelectPalette (srcdc, holdpal, FALSE);
  
  if (!ReleaseDC (src->handle, srcdc))
    WIN32_GDI_FAILED ("ReleaseDC");
}

void
_gdk_win32_blit (gboolean              use_fg_bg,
		 GdkDrawableImplWin32 *drawable,
		 GdkGC       	      *gc,
		 GdkDrawable 	      *src,
		 gint        	       xsrc,
		 gint        	       ysrc,
		 gint        	       xdest,
		 gint        	       ydest,
		 gint        	       width,
		 gint        	       height)
{
  HDC hdc;
  HRGN src_rgn, draw_rgn, outside_rgn;
  RECT r;
  GdkDrawableImplWin32 *draw_impl;
  GdkDrawableImplWin32 *src_impl;
  gint src_width, src_height;
  
  GDK_NOTE (MISC, g_print ("_gdk_win32_blit: src:%s %dx%d@+%d+%d\n"
			   "                 dst:%s @+%d+%d use_fg_bg=%d\n",
			   gdk_win32_drawable_description (src),
			   width, height, xsrc, ysrc,
			   gdk_win32_drawable_description ((GdkDrawable *) drawable),
			   xdest, ydest,
			   use_fg_bg));

  draw_impl = (GdkDrawableImplWin32 *) drawable;

  if (GDK_IS_DRAWABLE_IMPL_WIN32 (src))
    src_impl = (GdkDrawableImplWin32 *) src;
  else if (GDK_IS_WINDOW (src))
    src_impl = (GdkDrawableImplWin32 *) GDK_WINDOW_OBJECT (src)->impl;
  else if (GDK_IS_PIXMAP (src))
    src_impl = (GdkDrawableImplWin32 *) GDK_PIXMAP_OBJECT (src)->impl;
  else
    g_assert_not_reached ();

  hdc = gdk_win32_hdc_get ((GdkDrawable *) drawable, gc, GDK_GC_FOREGROUND);

  gdk_drawable_get_size (src, &src_width, &src_height);

  if ((src_rgn = CreateRectRgn (0, 0, src_width + 1, src_height + 1)) == NULL)
    WIN32_GDI_FAILED ("CreateRectRgn");
  else if ((draw_rgn = CreateRectRgn (xsrc, ysrc,
				      xsrc + width + 1,
				      ysrc + height + 1)) == NULL)
    WIN32_GDI_FAILED ("CreateRectRgn");
  else
    {
      if (GDK_IS_WINDOW_IMPL_WIN32 (draw_impl))
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
	      InvalidateRgn (draw_impl->handle, outside_rgn, TRUE);
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

  if (GDK_IS_PIXMAP_IMPL_WIN32 (src_impl))
    blit_from_pixmap (use_fg_bg, draw_impl, hdc,
		      (GdkPixmapImplWin32 *) src_impl, GDK_GC_WIN32 (gc),
		      xsrc, ysrc, xdest, ydest, width, height);
  else if (draw_impl->handle == src_impl->handle)
    blit_inside_window (src_impl, hdc, xsrc, ysrc, xdest, ydest, width, height);
  else
    blit_from_window (hdc, src_impl, xsrc, ysrc, xdest, ydest, width, height);
  gdk_win32_hdc_release ((GdkDrawable *) drawable, gc, GDK_GC_FOREGROUND);
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
  g_assert (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable));

  _gdk_win32_blit (TRUE, (GdkDrawableImplWin32 *) drawable,
		   gc, (GdkPixmap *) image->windowing_data,
		   xsrc, ysrc, xdest, ydest, width, height);
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

HGDIOBJ
gdk_win32_drawable_get_handle (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_HANDLE (drawable);
}
