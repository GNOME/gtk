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
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include "gdk.h"
#include "gdkprivate.h"


void
gdk_draw_point (GdkDrawable *drawable,
                GdkGC       *gc,
                gint         x,
                gint         y)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  XDrawPoint (drawable_private->xdisplay, drawable_private->xwindow,
              gc_private->xgc, x, y);
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

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  XDrawLine (drawable_private->xdisplay, drawable_private->xwindow,
	     gc_private->xgc, x1, y1, x2, y2);
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

  if (filled)
    XFillRectangle (drawable_private->xdisplay, drawable_private->xwindow,
		    gc_private->xgc, x, y, width, height);
  else
    XDrawRectangle (drawable_private->xdisplay, drawable_private->xwindow,
		    gc_private->xgc, x, y, width, height);
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

  if (filled)
    XFillArc (drawable_private->xdisplay, drawable_private->xwindow,
	      gc_private->xgc, x, y, width, height, angle1, angle2);
  else
    XDrawArc (drawable_private->xdisplay, drawable_private->xwindow,
	      gc_private->xgc, x, y, width, height, angle1, angle2);
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
  GdkPoint *local_points = points;
  gint local_npoints = npoints;
  gint local_alloc = 0;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  if (filled)
    {
      XFillPolygon (drawable_private->xdisplay, drawable_private->xwindow,
		    gc_private->xgc, (XPoint*) points, npoints, Complex, CoordModeOrigin);
    }
  else
    {
      if ((points[0].x != points[npoints-1].x) ||
        (points[0].y != points[npoints-1].y)) 
        {
          local_alloc = 1;
          ++local_npoints;
          local_points = (GdkPoint*) g_malloc (local_npoints * sizeof(GdkPoint));
          memcpy (local_points, points, npoints * sizeof(GdkPoint));
          local_points[npoints].x = points[0].x;
          local_points[npoints].y = points[0].y;
      }

      XDrawLines (drawable_private->xdisplay, drawable_private->xwindow,
                 gc_private->xgc,
                 (XPoint*) local_points, local_npoints,
                 CoordModeOrigin);
  
       if (local_alloc)
       g_free (local_points);
    }
}

/* gdk_draw_string
 *
 * Modified by Li-Da Lho to draw 16 bits and Multibyte strings
 *
 * Interface changed: add "GdkFont *font" to specify font or fontset explicitely
 */
void
gdk_draw_string (GdkDrawable *drawable,
		 GdkFont     *font,
		 GdkGC       *gc,
		 gint         x,
		 gint         y,
		 const gchar *string)
{
  GdkWindowPrivate *drawable_private;
  GdkFontPrivate *font_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (font != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (string != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;
  font_private = (GdkFontPrivate*) font;

  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) font_private->xfont;
      XSetFont(drawable_private->xdisplay, gc_private->xgc, xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawString (drawable_private->xdisplay, drawable_private->xwindow,
		       gc_private->xgc, x, y, string, strlen (string));
	}
      else
	{
	  XDrawString16 (drawable_private->xdisplay, drawable_private->xwindow,
			 gc_private->xgc, x, y, (XChar2b *) string,
			 strlen (string) / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) font_private->xfont;
      XmbDrawString (drawable_private->xdisplay, drawable_private->xwindow,
		     fontset, gc_private->xgc, x, y, string, strlen (string));
    }
  else
    g_error("undefined font type\n");
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
      XFontStruct *xfont = (XFontStruct *) font_private->xfont;
      XSetFont(drawable_private->xdisplay, gc_private->xgc, xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawString (drawable_private->xdisplay, drawable_private->xwindow,
		       gc_private->xgc, x, y, text, text_length);
	}
      else
	{
	  XDrawString16 (drawable_private->xdisplay, drawable_private->xwindow,
			 gc_private->xgc, x, y, (XChar2b *) text, text_length / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) font_private->xfont;
      XmbDrawString (drawable_private->xdisplay, drawable_private->xwindow,
		     fontset, gc_private->xgc, x, y, text, text_length);
    }
  else
    g_error("undefined font type\n");
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

  XCopyArea (drawable_private->xdisplay,
	     src_private->xwindow,
	     drawable_private->xwindow,
	     gc_private->xgc,
	     xsrc, ysrc,
	     width, height,
	     xdest, ydest);
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

  g_return_if_fail (drawable != NULL);
  g_return_if_fail ((points != NULL) && (npoints > 0));
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  XDrawPoints (drawable_private->xdisplay,
	       drawable_private->xwindow,
	       gc_private->xgc,
	       (XPoint *) points,
	       npoints,
	       CoordModeOrigin);
}

void
gdk_draw_segments (GdkDrawable *drawable,
		   GdkGC       *gc,
		   GdkSegment  *segs,
		   gint         nsegs)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;

  if (nsegs <= 0)
    return;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (segs != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  XDrawSegments (drawable_private->xdisplay,
		 drawable_private->xwindow,
		 gc_private->xgc,
		 (XSegment *) segs,
		 nsegs);
}

void
gdk_draw_lines (GdkDrawable *drawable,
              GdkGC       *gc,
              GdkPoint    *points,
              gint         npoints)
{
  GdkWindowPrivate *drawable_private;
  GdkGCPrivate *gc_private;

  if (npoints <= 0)
    return;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (points != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  gc_private = (GdkGCPrivate*) gc;

  XDrawLines (drawable_private->xdisplay,
            drawable_private->xwindow,
            gc_private->xgc,
            (XPoint *) points,
            npoints,
            CoordModeOrigin);
}
