#include "gdk.h"
#include "gdkprivate-nanox.h"

static void    gdk_nanox_drawable_destroy   (GdkDrawable     *drawable);

static void gdk_nanox_draw_rectangle (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gint            filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height);
static void gdk_nanox_draw_arc       (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gint            filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height,
				    gint            angle1,
				    gint            angle2);
static void gdk_nanox_draw_polygon   (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gint            filled,
				    GdkPoint       *points,
				    gint            npoints);
static void gdk_nanox_draw_text      (GdkDrawable    *drawable,
				    GdkFont        *font,
				    GdkGC          *gc,
				    gint            x,
				    gint            y,
				    const gchar    *text,
				    gint            text_length);
static void gdk_nanox_draw_text_wc   (GdkDrawable    *drawable,
				    GdkFont        *font,
				    GdkGC          *gc,
				    gint            x,
				    gint            y,
				    const GdkWChar *text,
				    gint            text_length);
static void gdk_nanox_draw_drawable  (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPixmap      *src,
				    gint            xsrc,
				    gint            ysrc,
				    gint            xdest,
				    gint            ydest,
				    gint            width,
				    gint            height);
static void gdk_nanox_draw_points    (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPoint       *points,
				    gint            npoints);
static void gdk_nanox_draw_segments  (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkSegment     *segs,
				    gint            nsegs);
static void gdk_nanox_draw_lines     (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPoint       *points,
				    gint            npoints);

GdkDrawableClass _gdk_nanox_drawable_class = {
  gdk_nanox_drawable_destroy,
  _gdk_nanox_gc_new,
  gdk_nanox_draw_rectangle,
  gdk_nanox_draw_arc,
  gdk_nanox_draw_polygon,
  gdk_nanox_draw_text,
  gdk_nanox_draw_text_wc,
  gdk_nanox_draw_drawable,
  gdk_nanox_draw_points,
  gdk_nanox_draw_segments,
  gdk_nanox_draw_lines
};

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
	{
	  /*XGetWindowAttributes (GDK_DRAWABLE_XDISPLAY (drawable),
				GDK_DRAWABLE_XID (drawable),
				&window_attributes);
	  drawable_private->colormap =  gdk_colormap_lookup (window_attributes.colormap);*/
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

	}

      if (drawable_private->colormap)
	gdk_colormap_unref (drawable_private->colormap);
      drawable_private->colormap = colormap;
      gdk_colormap_ref (drawable_private->colormap);

      if (GDK_IS_WINDOW (drawable) &&
	  drawable_private->window_type != GDK_WINDOW_TOPLEVEL)
	/*gdk_window_add_colormap_windows (drawable);*/;
    }
}

static void 
gdk_nanox_drawable_destroy (GdkDrawable *drawable)
{
  
}

static void
gdk_nanox_draw_rectangle (GdkDrawable *drawable,
			GdkGC       *gc,
			gint         filled,
			gint         x,
			gint         y,
			gint         width,
			gint         height)
{
  if (filled)
    GrFillRect (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), x, y, width, height);
  else
    GrRect (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), x, y, width, height);
}

static void
gdk_nanox_draw_arc (GdkDrawable *drawable,
		  GdkGC       *gc,
		  gint         filled,
		  gint         x,
		  gint         y,
		  gint         width,
		  gint         height,
		  gint         angle1,
		  gint         angle2)
{
		/* this is not an arc, obviously */
  if (filled)
    GrFillEllipse (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), x, y, width/2, height/2);
  else
    GrEllipse (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), x, y, width/2, height/2);
}

static void
gdk_nanox_draw_polygon (GdkDrawable *drawable,
		      GdkGC       *gc,
		      gint         filled,
		      GdkPoint    *points,
		      gint         npoints)
{
  GR_POINT *new_points = g_new(GR_POINT, npoints);
  int i;
  for (i=0; i < npoints;++i) {
    new_points[i].x = points[i].x;
    new_points[i].y = points[i].y;
  }
  if (filled)
    {
      GrFillPoly (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), npoints, new_points);
    }
  else
    {
      GrPoly (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), npoints, new_points);
    }
  g_free(new_points);
}

static void
gdk_nanox_draw_text (GdkDrawable *drawable,
		   GdkFont     *font,
		   GdkGC       *gc,
		   gint         x,
		   gint         y,
		   const gchar *text,
		   gint         text_length)
{
  GrSetGCFont(GDK_GC_XGC(gc), GDK_FONT_XFONT(font));
  GrText (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), x, y, text, text_length, TF_UTF8|TF_BASELINE);
}

static void
gdk_nanox_draw_text_wc (GdkDrawable    *drawable,
		      GdkFont	     *font,
		      GdkGC	     *gc,
		      gint	      x,
		      gint	      y,
		      const GdkWChar *text,
		      gint	      text_length)
{
  GrSetGCFont(GDK_GC_XGC(gc), GDK_FONT_XFONT(font));
  GrText (GDK_DRAWABLE_XID (drawable), GDK_GC_XGC (gc), x, y, text, text_length, TF_UC32|TF_BASELINE);
}

static void
gdk_nanox_draw_drawable (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPixmap   *src,
		       gint         xsrc,
		       gint         ysrc,
		       gint         xdest,
		       gint         ydest,
		       gint         width,
		       gint         height)
{
  GrCopyArea(GDK_DRAWABLE_XID(drawable), GDK_GC_XGC(gc), xdest, ydest,
	width, height, GDK_DRAWABLE_XID(src), xsrc, ysrc, 0);
}

static void
gdk_nanox_draw_points (GdkDrawable *drawable,
		     GdkGC       *gc,
		     GdkPoint    *points,
		     gint         npoints)
{
  int i;
  for (i=0; i < npoints; ++i)
    GrPoint(GDK_DRAWABLE_XID(drawable), GDK_GC_XGC(gc), points[i].x, points[i].y);
}

static void
gdk_nanox_draw_segments (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkSegment  *segs,
		       gint         nsegs)
{
  int i;
  for (i=0; i < nsegs; ++i)
	GrLine(GDK_DRAWABLE_XID(drawable), GDK_GC_XGC(gc), segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
}

static void
gdk_nanox_draw_lines (GdkDrawable *drawable,
		    GdkGC       *gc,
		    GdkPoint    *points,
		    gint         npoints)
{
  int i;
  for (i=0; i < npoints-1; ++i)
	GrLine(GDK_DRAWABLE_XID(drawable), GDK_GC_XGC(gc), points[i].x, points[i].y, points[i+1].x, points[i+1].y);
}

