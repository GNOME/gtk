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
#include <gdk/gdk.h>
#include "gdkprivate.h"

#ifndef M_TWOPI
#define M_TWOPI         (2.0 * 3.14159265358979323846)
#endif

void
gdk_draw_point (GdkDrawable *drawable,
                GdkGC       *gc,
                gint         x,
                gint         y)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable_private, gc_private);

  /* We use LineTo because SetPixel wants the COLORREF directly,
   * and doesn't use the current pen, which is what we want.
   */
  if (!MoveToEx (hdc, x, y, NULL))
    g_warning ("gdk_draw_point: MoveToEx failed");
  if (!LineTo (hdc, x + 1, y))
    g_warning ("gdk_draw_point: LineTo failed");
  
  gdk_gc_postdraw (drawable_private, gc_private);
}

void
gdk_draw_line (GdkDrawable *drawable,
	       GdkGC       *gc,
	       gint         x1,
	       gint         y1,
	       gint         x2,
	       gint         y2)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable_private, gc_private);
    
  GDK_NOTE (MISC, g_print ("gdk_draw_line: %#x (%d) +%d+%d..+%d+%d\n",
			   drawable_private->xwindow, gc_private,
			   x1, y1, x2, y2));
  
  MoveToEx (hdc, x1, y1, NULL);
  if (!LineTo (hdc, x2, y2))
    g_warning ("gdk_draw_line: LineTo #1 failed");
  /* LineTo doesn't draw the last point, so if we have a pen width of 1,
   * we draw the end pixel separately... With wider pens it hopefully
   * doesn't matter?
   */
  if (gc_private->pen_width == 1)
    if (!LineTo (hdc, x2 + 1, y2))
      g_warning ("gdk_draw_line: LineTo #2 failed");
  gdk_gc_postdraw (drawable_private, gc_private);
}

void
gdk_draw_rectangle (GdkDrawable *drawable,
		    GdkGC       *gc,
		    gint         filled,
		    gint         x,
		    gint         y,
		    gint         width,
		    gint         height)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  HGDIOBJ oldpen, oldbrush;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  if (width == -1)
    width = drawable_private->width;
  if (height == -1)
    height = drawable_private->height;

  hdc = gdk_gc_predraw (drawable_private, gc_private);

  GDK_NOTE (MISC, g_print ("gdk_draw_rectangle: %#x (%d) %s%dx%d@+%d+%d\n",
			   drawable_private->xwindow,
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

  gdk_gc_postdraw (drawable_private, gc_private);
}

void
gdk_draw_arc (GdkDrawable *drawable,
	      GdkGC       *gc,
	      gint         filled,
	      gint         x,
	      gint         y,
	      gint         width,
	      gint         height,
	      gint         angle1,
	      gint         angle2)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  int nXStartArc, nYStartArc, nXEndArc, nYEndArc;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  if (width == -1)
    width = drawable_private->width;
  if (height == -1)
    height = drawable_private->height;

  hdc = gdk_gc_predraw (drawable_private, gc_private);

  nXStartArc = x + width/2 + (int) (sin(angle1/64.*M_TWOPI)*width);
  nYStartArc = y + height/2 + (int) (cos(angle1/64.*M_TWOPI)*height);
  nXEndArc = x + width/2 + (int) (sin(angle2/64.*M_TWOPI)*width);
  nYEndArc = y + height/2 + (int) (cos(angle2/64.*M_TWOPI)*height);

  if (filled)
    {
      if (!Pie (hdc, x, y, x+width, y+height,
		nXStartArc, nYStartArc, nXEndArc, nYEndArc))
	g_warning ("gdk_draw_arc: Pie failed");
    }
  else
    {
      if (!Arc (hdc, x, y, x+width, y+height,
		nXStartArc, nYStartArc, nXEndArc, nYEndArc))
	g_warning ("gdk_draw_arc: Arc failed");
    }
  gdk_gc_postdraw (drawable_private, gc_private);
}

void
gdk_draw_polygon (GdkDrawable *drawable,
		  GdkGC       *gc,
		  gint         filled,
		  GdkPoint    *points,
		  gint         npoints)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  POINT *pts;
  int i;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable_private, gc_private);
  pts = g_malloc ((npoints+1) * sizeof (POINT));

  GDK_NOTE (MISC, g_print ("gdk_draw_polygon: %#x (%d) %d\n",
			   drawable_private->xwindow, gc_private,
			   npoints));

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
  gdk_gc_postdraw (drawable_private, gc_private);
}

/* gdk_draw_string
 */
void
gdk_draw_string (GdkDrawable *drawable,
		 GdkFont     *font,
		 GdkGC       *gc,
		 gint         x,
		 gint         y,
		 const gchar *string)
{
  gdk_draw_text (drawable, font, gc, x, y, string, strlen (string));
}

/* gdk_draw_text
 *
 * Modified by Li-Da Lho to draw 16 bits and Multibyte strings
 *
 * Interface changed: add "GdkFont *font" to specify font or fontset explicitely
 */
void
gdk_draw_text (GdkDrawable *drawable,
	       GdkFont     *font,
	       GdkGC       *gc,
	       gint         x,
	       gint         y,
	       const gchar *text,
	       gint         text_length)
{
  GdkWindowPrivate *drawable_private;
  GdkFontPrivate *font_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  HFONT xfont;
  HGDIOBJ oldfont;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (font != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (text != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;
  font_private = (GdkFontPrivate*) font;

  if (font->type == GDK_FONT_FONT)
    {
      hdc = gdk_gc_predraw (drawable_private, gc_private);
      xfont = (HFONT) font_private->xfont;

      GDK_NOTE (MISC, g_print ("gdk_draw_text: %#x (%d) %#x "
			       "+%d+%d font: %#x \"%.*s\" length: %d\n",
			       drawable_private->xwindow,
			       gc_private, gc_private->xgc,
			       x, y, xfont,
			       (text_length > 10 ? 10 : text_length),
			       text, text_length));
      
      if ((oldfont = SelectObject (hdc, xfont)) == NULL)
	g_warning ("gdk_draw_text: SelectObject failed");
      if (!TextOutA (hdc, x, y, text, text_length))
	g_warning ("gdk_draw_text: TextOutA failed");
      SelectObject (hdc, oldfont);
      gdk_gc_postdraw (drawable_private, gc_private);
    }
  else
    g_error ("undefined font type");
}

void
gdk_draw_text_wc (GdkDrawable	 *drawable,
		  GdkFont	 *font,
		  GdkGC		 *gc,
		  gint		  x,
		  gint		  y,
		  const GdkWChar *text,
		  gint		  text_length)
{
  GdkWindowPrivate *drawable_private;
  GdkFontPrivate *font_private;
  GdkGCPrivate *gc_private;
  gint i;
  wchar_t *wcstr;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (font != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (text != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;
  font_private = (GdkFontPrivate*) font;

  if (font->type == GDK_FONT_FONT)
    {
      HDC hdc;
      HFONT xfont;
      HGDIOBJ oldfont;

      hdc = gdk_gc_predraw (drawable_private, gc_private);
      xfont = (HFONT) font_private->xfont;

      GDK_NOTE (MISC, g_print ("gdk_draw_text_wc: %#x (%d) %#x "
			       "+%d+%d font: %#x length: %d\n",
			       drawable_private->xwindow,
			       gc_private, gc_private->xgc,
			       x, y, xfont,
			       text_length));
      
      if ((oldfont = SelectObject (hdc, xfont)) == NULL)
	g_warning ("gdk_draw_text: SelectObject failed");
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
      if (!TextOutW (hdc, x, y, wcstr, text_length))
	g_warning ("gdk_draw_text: TextOutW failed");
      g_free (wcstr);
      SelectObject (hdc, oldfont);
      gdk_gc_postdraw (drawable_private, gc_private);
    }
  else
    g_error ("undefined font type");
}

void
gdk_draw_pixmap (GdkDrawable *drawable,
		 GdkGC       *gc,
		 GdkPixmap   *src,
		 gint         xsrc,
		 gint         ysrc,
		 gint         xdest,
		 gint         ydest,
		 gint         width,
		 gint         height)
{
  GdkWindowPrivate *drawable_private;
  GdkWindowPrivate *src_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  HDC srcdc;
  HGDIOBJ hgdiobj;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (src != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  src_private = (GdkWindowPrivate*) src;
  if (drawable_private->destroyed || src_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  if (width == -1)
    width = src_private->width;
  if (height == -1)
    height = src_private->height;

  hdc = gdk_gc_predraw (drawable_private, gc_private);

  GDK_NOTE (MISC, g_print ("gdk_draw_pixmap: dest: %#x destdc: (%d) %#x "
			   "src: %#x %dx%d@+%d+%d\n",
			   drawable_private->xwindow, gc_private, hdc,
			   src_private->xwindow,
			   width, height, xdest, ydest));

  /* Strangely enough, this function is called also to bitblt
   * from a window.
   */
  if (src_private->window_type == GDK_WINDOW_PIXMAP)
    {
      if ((srcdc = CreateCompatibleDC (hdc)) == NULL)
	g_warning ("gdk_draw_pixmap: CreateCompatibleDC failed");
      
      if ((hgdiobj = SelectObject (srcdc, src_private->xwindow)) == NULL)
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
      if ((srcdc = GetDC (src_private->xwindow)) == NULL)
	g_warning ("gdk_draw_pixmap: GetDC failed");
      
      if (!BitBlt (hdc, xdest, ydest, width, height,
		   srcdc, xsrc, ysrc, SRCCOPY))
	g_warning ("gdk_draw_pixmap: BitBlt failed");
      
      ReleaseDC (src_private->xwindow, srcdc);
    }
  gdk_gc_postdraw (drawable_private, gc_private);
}

void
gdk_draw_image (GdkDrawable *drawable,
		GdkGC       *gc,
		GdkImage    *image,
		gint         xsrc,
		gint         ysrc,
		gint         xdest,
		gint         ydest,
		gint         width,
		gint         height)
{
  GdkImagePrivate *image_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (gc != NULL);

  image_private = (GdkImagePrivate*) image;

  g_return_if_fail (image_private->image_put != NULL);

  if (width == -1)
    width = image->width;
  if (height == -1)
    height = image->height;

  (* image_private->image_put) (drawable, gc, image, xsrc, ysrc,
				xdest, ydest, width, height);
}

void
gdk_draw_points (GdkDrawable *drawable,
		 GdkGC       *gc,
		 GdkPoint    *points,
		 gint         npoints)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  int i;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail ((points != NULL) && (npoints > 0));
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable_private, gc_private);
  
  GDK_NOTE (MISC, g_print ("gdk_draw_points: %#x destdc: (%d) %#x "
			   "npoints: %d\n",
			   drawable_private->xwindow, gc_private, hdc,
			   npoints));

  for (i = 0; i < npoints; i++)
    {
      if (!MoveToEx (hdc, points[i].x, points[i].y, NULL))
	g_warning ("gdk_draw_points: MoveToEx failed");
      if (!LineTo (hdc, points[i].x + 1, points[i].y))
	g_warning ("gdk_draw_points: LineTo failed");
    }
  gdk_gc_postdraw (drawable_private, gc_private);
}

void
gdk_draw_segments (GdkDrawable *drawable,
		   GdkGC       *gc,
		   GdkSegment  *segs,
		   gint         nsegs)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  int i;

  if (nsegs <= 0)
    return;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (segs != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable_private, gc_private);

  for (i = 0; i < nsegs; i++)
    {
      if (!MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
	g_warning ("gdk_draw_segments: MoveToEx failed");
      if (!LineTo (hdc, segs[i].x2, segs[i].y2))
	g_warning ("gdk_draw_segments: LineTo #1 failed");
      
      /* Draw end pixel */
      if (gc_private->pen_width == 1)
	if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
	  g_warning ("gdk_draw_segments: LineTo #2 failed");
    }
  gdk_gc_postdraw (drawable_private, gc_private);
}

void
gdk_draw_lines (GdkDrawable *drawable,
		GdkGC       *gc,
		GdkPoint    *points,
		gint         npoints)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;
  HDC hdc;
  POINT *pts;
  int i;

  if (npoints <= 0)
    return;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (points != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  gc_private = (GdkGCPrivate*) gc;

  hdc = gdk_gc_predraw (drawable_private, gc_private);
#if 1
  pts = g_malloc (npoints * sizeof (POINT));

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if (!Polyline (hdc, pts, npoints))
    g_warning ("gdk_draw_lines: Polyline failed");
  
  g_free (pts);
  
  /* Draw end pixel */
  if (gc_private->pen_width == 1)
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
  if (gc_private->pen_width == 1)
    if (!LineTo (hdc, points[npoints-1].x + 1, points[npoints-1].y))
      g_warning ("gdk_draw_lines: LineTo #2 failed");
#endif	
  gdk_gc_postdraw (drawable_private, gc_private);
}
