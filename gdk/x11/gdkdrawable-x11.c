#include "gdkx.h"

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
		    GDK_GC_XGC (gc), x, y, width, height);
  else
    XDrawRectangle (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		    GDK_GC_XGC (gc), x, y, width, height);
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
	      GDK_GC_XGC (gc), x, y, width, height, angle1, angle2);
  else
    XDrawArc (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
	      GDK_GC_XGC (gc), x, y, width, height, angle1, angle2);
}

static void
gdk_x11_draw_polygon (GdkDrawable *drawable,
		      GdkGC       *gc,
		      gint         filled,
		      GdkPoint    *points,
		      gint         npoints)
{
  if (filled)
    {
      XFillPolygon (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		    GDK_GC_XGC (gc), (XPoint*) points, npoints, Complex, CoordModeOrigin);
    }
  else
    {
      GdkPoint *local_points = points;
      gint local_npoints = npoints;
      gint local_alloc = 0;

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

      XDrawLines (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		  GDK_GC_XGC (gc),
		  (XPoint*) local_points, local_npoints,
		  CoordModeOrigin);
  
       if (local_alloc)
       g_free (local_points);
    }
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
      XSetFont(GDK_DRAWABLE_XDISPLAY (drawable), GDK_GC_XGC (gc), xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		       GDK_GC_XGC (gc), x, y, text, text_length);
	}
      else
	{
	  XDrawString16 (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
			 GDK_GC_XGC (gc), x, y, (XChar2b *) text, text_length / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) GDK_FONT_XFONT (font);
      XmbDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
		     fontset, GDK_GC_XGC (gc), x, y, text, text_length);
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
      XSetFont(GDK_DRAWABLE_XDISPLAY (drawable), GDK_GC_XGC (gc), xfont->fid);
      text_8bit = g_new (gchar, text_length);
      for (i=0; i<text_length; i++) text_8bit[i] = text[i];
      XDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
                   GDK_GC_XGC (gc), x, y, text_8bit, text_length);
      g_free (text_8bit);
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      if (sizeof(GdkWChar) == sizeof(wchar_t))
	{
	  XwcDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_XGC (gc), x, y, (wchar_t *)text, text_length);
	}
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  text_wchar = g_new (wchar_t, text_length);
	  for (i=0; i<text_length; i++) text_wchar[i] = text[i];
	  XwcDrawString (GDK_DRAWABLE_XDISPLAY (drawable), GDK_DRAWABLE_XID (drawable),
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_XGC (gc), x, y, text_wchar, text_length);
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
  /* FIXME: this doesn't work because bitmaps don't have visuals */
  if (gdk_drawable_get_visual (src)->depth == 1)
    {
      XCopyArea (GDK_DRAWABLE_XDISPLAY (drawable),
		 GDK_DRAWABLE_XID (src),
		 GDK_DRAWABLE_XID (drawable),
		 GDK_GC_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else
    {
      XCopyArea (GDK_DRAWABLE_XDISPLAY (drawable),
		 GDK_DRAWABLE_XID (src),
		 GDK_DRAWABLE_XID (drawable),
		 GDK_GC_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
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
		  GDK_GC_XGC (gc),
		  points[0].x, points[0].y);
    }
  else
    {
      XDrawPoints (GDK_DRAWABLE_XDISPLAY (drawable),
		   GDK_DRAWABLE_XID (drawable),
		   GDK_GC_XGC (gc),
		   (XPoint *) points,
		   npoints,
		   CoordModeOrigin);
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
		 GDK_GC_XGC (gc), segs[0].x1, segs[0].y1,
		 segs[0].x2, segs[0].y2);
    }
  else
    {
      XDrawSegments (GDK_DRAWABLE_XDISPLAY (drawable),
		     GDK_DRAWABLE_XID (drawable),
		     GDK_GC_XGC (gc),
		     (XSegment *) segs,
		     nsegs);
    }
}

static void
gdk_x11_draw_lines (GdkDrawable *drawable,
		    GdkGC       *gc,
		    GdkPoint    *points,
		    gint         npoints)
{
  XDrawLines (GDK_DRAWABLE_XDISPLAY (drawable),
	      GDK_DRAWABLE_XID (drawable),
	      GDK_GC_XGC (gc),
	      (XPoint *) points,
	      npoints,
	      CoordModeOrigin);
}
