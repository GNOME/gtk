#include "gdkgc.h"
#include "gdkx.h"
#include "gdkregion-generic.h"

typedef enum {
  GDK_GC_DIRTY_CLIP = 1 << 0,
  GDK_GC_DIRTY_TS = 1 << 1
} GdkGCDirtyValues;

static void gdk_x11_gc_values_to_xvalues (GdkGCValues    *values,
					  GdkGCValuesMask mask,
					  XGCValues      *xvalues,
					  unsigned long  *xvalues_mask,
					  gboolean        initial);
     
static void gdk_x11_gc_destroy    (GdkGC           *gc);
static void gdk_x11_gc_get_values (GdkGC           *gc,
				   GdkGCValues     *values);
static void gdk_x11_gc_set_values (GdkGC           *gc,
				   GdkGCValues     *values,
				   GdkGCValuesMask  values_mask);
static void gdk_x11_gc_set_dashes (GdkGC           *gc,
				   gint             dash_offset,
				   gchar            dash_list[],
				   gint             n);

static GdkGCClass gdk_x11_gc_class = {
  gdk_x11_gc_destroy,
  gdk_x11_gc_get_values,
  gdk_x11_gc_set_values,
  gdk_x11_gc_set_dashes
};

GdkGC *
_gdk_x11_gc_new (GdkDrawable      *drawable,
		 GdkGCValues      *values,
		 GdkGCValuesMask   values_mask)
{
  GdkGC *gc;
  GdkGCPrivate *private;
  GdkGCXData *data;
  
  XGCValues xvalues;
  unsigned long xvalues_mask;

  gc = gdk_gc_alloc ();
  private = (GdkGCPrivate *)gc;

  private->klass = &gdk_x11_gc_class;
  private->klass_data = data = g_new (GdkGCXData, 1);

  data->dirty_mask = 0;
  data->clip_region = NULL;
    
  GDK_GC_XDATA (gc)->xdisplay = GDK_DRAWABLE_XDISPLAY (drawable);

  if (values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
      data->dirty_mask |= GDK_GC_DIRTY_CLIP;
    }

  if (values_mask & (GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
      data->dirty_mask |= GDK_GC_DIRTY_TS;
    }

  xvalues.function = GXcopy;
  xvalues.fill_style = FillSolid;
  xvalues.arc_mode = ArcPieSlice;
  xvalues.subwindow_mode = ClipByChildren;
  xvalues.graphics_exposures = True;
  xvalues_mask = GCFunction | GCFillStyle | GCArcMode | GCSubwindowMode | GCGraphicsExposures;

  gdk_x11_gc_values_to_xvalues (values, values_mask, &xvalues, &xvalues_mask, TRUE);
  
  data->xgc = XCreateGC (GDK_GC_XDISPLAY (gc),
			 GDK_DRAWABLE_XID (drawable),
			 xvalues_mask, &xvalues);

  return gc;
}

static void
gdk_x11_gc_destroy (GdkGC *gc)
{
  if (GDK_GC_XDATA (gc)->clip_region)
    gdk_region_destroy (GDK_GC_XDATA (gc)->clip_region);
  
  XFreeGC (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc));
  g_free (GDK_GC_XDATA (gc));
}

GC
_gdk_x11_gc_flush (GdkGC *gc)
{
  GdkGCPrivate *private = (GdkGCPrivate *)gc;
  GdkGCXData *data = GDK_GC_XDATA (gc);

  if (data->dirty_mask & GDK_GC_DIRTY_CLIP)
    {
      if (!data->clip_region)
	XSetClipOrigin (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc),
			private->clip_x_origin, private->clip_y_origin);
      else
	{
	  XRectangle *rectangles = g_new (XRectangle, data->clip_region->numRects);
	  GdkRegionBox *boxes = data->clip_region->rects;
	  int i;

	  for (i=0; i<data->clip_region->numRects; i++)
	    {
	      rectangles[i].x = CLAMP (boxes[i].x1 + private->clip_x_origin, G_MINSHORT, G_MAXSHORT);
	      rectangles[i].y = CLAMP (boxes[i].y1 + private->clip_y_origin, G_MINSHORT, G_MAXSHORT);
	      rectangles[i].width = CLAMP (boxes[i].x2 + private->clip_x_origin, G_MINSHORT, G_MAXSHORT) - rectangles[i].x;
	      rectangles[i].height = CLAMP (boxes[i].y2 + private->clip_y_origin, G_MINSHORT, G_MAXSHORT) - rectangles[i].y;
	    }
	  
	  XSetClipRectangles(GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc), 0, 0, rectangles,
			     data->clip_region->numRects, YXBanded);

	  g_free (rectangles);
	}
    }

  if (data->dirty_mask & GDK_GC_DIRTY_TS)
    {
      XSetTSOrigin (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc),
		    private->ts_x_origin, private->ts_y_origin);
    }

  data->dirty_mask = 0;
  return GDK_GC_XGC (gc);
}

static void
gdk_x11_gc_get_values (GdkGC       *gc,
		       GdkGCValues *values)
{
  XGCValues xvalues;
  
  if (XGetGCValues (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc),
		    GCForeground | GCBackground | GCFont |
		    GCFunction | GCTile | GCStipple | /* GCClipMask | */
		    GCSubwindowMode | GCGraphicsExposures |
		    GCTileStipXOrigin | GCTileStipYOrigin |
		    GCClipXOrigin | GCClipYOrigin |
		    GCLineWidth | GCLineStyle | GCCapStyle |
		    GCFillStyle | GCJoinStyle, &xvalues))
    {
      values->foreground.pixel = xvalues.foreground;
      values->background.pixel = xvalues.background;
      values->font = gdk_font_lookup (xvalues.font);

      switch (xvalues.function)
	{
	case GXcopy:
	  values->function = GDK_COPY;
	  break;
	case GXinvert:
	  values->function = GDK_INVERT;
	  break;
	case GXxor:
	  values->function = GDK_XOR;
	  break;
	case GXclear:
	  values->function = GDK_CLEAR;
	  break;
	case GXand:
	  values->function = GDK_AND;
	  break;
	case GXandReverse:
	  values->function = GDK_AND_REVERSE;
	  break;
	case GXandInverted:
	  values->function = GDK_AND_INVERT;
	  break;
	case GXnoop:
	  values->function = GDK_NOOP;
	  break;
	case GXor:
	  values->function = GDK_OR;
	  break;
	case GXequiv:
	  values->function = GDK_EQUIV;
	  break;
	case GXorReverse:
	  values->function = GDK_OR_REVERSE;
	  break;
	case GXcopyInverted:
	  values->function =GDK_COPY_INVERT;
	  break;
	case GXorInverted:
	  values->function = GDK_OR_INVERT;
	  break;
	case GXnand:
	  values->function = GDK_NAND;
	  break;
	case GXset:
	  values->function = GDK_SET;
	  break;
	case GXnor:
	  values->function = GDK_NOR;
	  break;
	}

      switch (xvalues.fill_style)
	{
	case FillSolid:
	  values->fill = GDK_SOLID;
	  break;
	case FillTiled:
	  values->fill = GDK_TILED;
	  break;
	case FillStippled:
	  values->fill = GDK_STIPPLED;
	  break;
	case FillOpaqueStippled:
	  values->fill = GDK_OPAQUE_STIPPLED;
	  break;
	}

      values->tile = gdk_pixmap_lookup (xvalues.tile);
      values->stipple = gdk_pixmap_lookup (xvalues.stipple);
      values->clip_mask = NULL;
      values->subwindow_mode = xvalues.subwindow_mode;
      values->ts_x_origin = xvalues.ts_x_origin;
      values->ts_y_origin = xvalues.ts_y_origin;
      values->clip_x_origin = xvalues.clip_x_origin;
      values->clip_y_origin = xvalues.clip_y_origin;
      values->graphics_exposures = xvalues.graphics_exposures;
      values->line_width = xvalues.line_width;

      switch (xvalues.line_style)
	{
	case LineSolid:
	  values->line_style = GDK_LINE_SOLID;
	  break;
	case LineOnOffDash:
	  values->line_style = GDK_LINE_ON_OFF_DASH;
	  break;
	case LineDoubleDash:
	  values->line_style = GDK_LINE_DOUBLE_DASH;
	  break;
	}

      switch (xvalues.cap_style)
	{
	case CapNotLast:
	  values->cap_style = GDK_CAP_NOT_LAST;
	  break;
	case CapButt:
	  values->cap_style = GDK_CAP_BUTT;
	  break;
	case CapRound:
	  values->cap_style = GDK_CAP_ROUND;
	  break;
	case CapProjecting:
	  values->cap_style = GDK_CAP_PROJECTING;
	  break;
	}

      switch (xvalues.join_style)
	{
	case JoinMiter:
	  values->join_style = GDK_JOIN_MITER;
	  break;
	case JoinRound:
	  values->join_style = GDK_JOIN_ROUND;
	  break;
	case JoinBevel:
	  values->join_style = GDK_JOIN_BEVEL;
	  break;
	}
    }
  else
    {
      memset (values, 0, sizeof (GdkGCValues));
    }
}


static void
gdk_x11_gc_set_values (GdkGC           *gc,
		       GdkGCValues     *values,
		       GdkGCValuesMask  values_mask)
{
  GdkGCXData *data;
  XGCValues xvalues;
  unsigned long xvalues_mask = 0;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_XDATA (gc);

  if (values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
      data->dirty_mask |= GDK_GC_DIRTY_CLIP;
    }

  if (values_mask & (GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
      data->dirty_mask |= GDK_GC_DIRTY_TS;
    }

  if (values_mask & GDK_GC_CLIP_MASK)
    {
      if (data->clip_region)
	{
	  gdk_region_destroy (data->clip_region);
	  data->clip_region = NULL;
	}
    }

  gdk_x11_gc_values_to_xvalues (values, values_mask, &xvalues, &xvalues_mask, FALSE);

  XChangeGC (GDK_GC_XDISPLAY (gc),
	     GDK_GC_XGC (gc),
	     xvalues_mask,
	     &xvalues);
}

static void
gdk_x11_gc_set_dashes (GdkGC *gc,
		       gint   dash_offset,
		       gchar  dash_list[],
		       gint   n)
{
  g_return_if_fail (gc != NULL);
  g_return_if_fail (dash_list != NULL);

  XSetDashes (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc),
	      dash_offset, dash_list, n);
}

static void
gdk_x11_gc_values_to_xvalues (GdkGCValues    *values,
			      GdkGCValuesMask mask,
			      XGCValues      *xvalues,
			      unsigned long  *xvalues_mask,
			      gboolean        initial)
{
  if (mask & GDK_GC_FOREGROUND)
    {
      xvalues->foreground = values->foreground.pixel;
      *xvalues_mask |= GCForeground;
    }
  if (mask & GDK_GC_BACKGROUND)
    {
      xvalues->background = values->background.pixel;
      *xvalues_mask |= GCBackground;
    }
  if ((mask & GDK_GC_FONT) && (values->font->type == GDK_FONT_FONT))
    {
      xvalues->font = ((XFontStruct *) (GDK_FONT_XFONT (values->font)))->fid;
      *xvalues_mask |= GCFont;
    }
  if (mask & GDK_GC_FUNCTION)
    {
      switch (values->function)
	{
	case GDK_COPY:
	  xvalues->function = GXcopy;
	  break;
	case GDK_INVERT:
	  xvalues->function = GXinvert;
	  break;
	case GDK_XOR:
	  xvalues->function = GXxor;
	  break;
	case GDK_CLEAR:
	  xvalues->function = GXclear;
	  break;
	case GDK_AND:
	  xvalues->function = GXand;
	  break;
	case GDK_AND_REVERSE:
	  xvalues->function = GXandReverse;
	  break;
	case GDK_AND_INVERT:
	  xvalues->function = GXandInverted;
	  break;
	case GDK_NOOP:
	  xvalues->function = GXnoop;
	  break;
	case GDK_OR:
	  xvalues->function = GXor;
	  break;
	case GDK_EQUIV:
	  xvalues->function = GXequiv;
	  break;
	case GDK_OR_REVERSE:
	  xvalues->function = GXorReverse;
	  break;
	case GDK_COPY_INVERT:
	  xvalues->function = GXcopyInverted;
	  break;
	case GDK_OR_INVERT:
	  xvalues->function = GXorInverted;
	  break;
	case GDK_NAND:
	  xvalues->function = GXnand;
	  break;
	case GDK_SET:
	  xvalues->function = GXset;
	  break;
	case GDK_NOR:
	  xvalues->function = GXnor;
	  break;
	}
      *xvalues_mask |= GCFunction;
    }
  if (mask & GDK_GC_FILL)
    {
      switch (values->fill)
	{
	case GDK_SOLID:
	  xvalues->fill_style = FillSolid;
	  break;
	case GDK_TILED:
	  xvalues->fill_style = FillTiled;
	  break;
	case GDK_STIPPLED:
	  xvalues->fill_style = FillStippled;
	  break;
	case GDK_OPAQUE_STIPPLED:
	  xvalues->fill_style = FillOpaqueStippled;
	  break;
	}
      *xvalues_mask |= GCFillStyle;
    }
  if (mask & GDK_GC_TILE)
    {
      if (values->tile)
	xvalues->tile = GDK_DRAWABLE_XID (values->tile);
      else
	xvalues->tile = None;
      
      *xvalues_mask |= GCTile;
    }
  if (mask & GDK_GC_STIPPLE)
    {
      if (values->stipple)
	xvalues->stipple = GDK_DRAWABLE_XID (values->stipple);
      else
	xvalues->stipple = None;
      
      *xvalues_mask |= GCStipple;
    }
  if (mask & GDK_GC_CLIP_MASK)
    {
      if (values->clip_mask)
	xvalues->clip_mask = GDK_DRAWABLE_XID (values->clip_mask);
      else
	xvalues->clip_mask = None;

      *xvalues_mask |= GCClipMask;
      
    }
  if (mask & GDK_GC_SUBWINDOW)
    {
      xvalues->subwindow_mode = values->subwindow_mode;
      *xvalues_mask |= GCSubwindowMode;
    }
  if (mask & GDK_GC_TS_X_ORIGIN)
    {
      xvalues->ts_x_origin = values->ts_x_origin;
      *xvalues_mask |= GCTileStipXOrigin;
    }
  if (mask & GDK_GC_TS_Y_ORIGIN)
    {
      xvalues->ts_y_origin = values->ts_y_origin;
      *xvalues_mask |= GCTileStipYOrigin;
    }
  if (mask & GDK_GC_CLIP_X_ORIGIN)
    {
      xvalues->clip_x_origin = values->clip_x_origin;
      *xvalues_mask |= GCClipXOrigin;
    }
  if (mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      xvalues->clip_y_origin = values->clip_y_origin;
      *xvalues_mask |= GCClipYOrigin;
    }

  if (mask & GDK_GC_EXPOSURES)
    {
      xvalues->graphics_exposures = values->graphics_exposures;
      *xvalues_mask |= GCGraphicsExposures;
    }
  else if (initial)
    {
      xvalues->graphics_exposures = False;
      *xvalues_mask |= GCGraphicsExposures;
    }

  if (mask & GDK_GC_LINE_WIDTH)
    {
      xvalues->line_width = values->line_width;
      *xvalues_mask |= GCLineWidth;
    }
  if (mask & GDK_GC_LINE_STYLE)
    {
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
	  xvalues->line_style = LineSolid;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	  xvalues->line_style = LineOnOffDash;
	  break;
	case GDK_LINE_DOUBLE_DASH:
	  xvalues->line_style = LineDoubleDash;
	  break;
	}
      *xvalues_mask |= GCLineStyle;
    }
  if (mask & GDK_GC_CAP_STYLE)
    {
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:
	  xvalues->cap_style = CapNotLast;
	  break;
	case GDK_CAP_BUTT:
	  xvalues->cap_style = CapButt;
	  break;
	case GDK_CAP_ROUND:
	  xvalues->cap_style = CapRound;
	  break;
	case GDK_CAP_PROJECTING:
	  xvalues->cap_style = CapProjecting;
	  break;
	}
      *xvalues_mask |= GCCapStyle;
    }
  if (mask & GDK_GC_JOIN_STYLE)
    {
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  xvalues->join_style = JoinMiter;
	  break;
	case GDK_JOIN_ROUND:
	  xvalues->join_style = JoinRound;
	  break;
	case GDK_JOIN_BEVEL:
	  xvalues->join_style = JoinBevel;
	  break;
	}
      *xvalues_mask |= GCJoinStyle;
    }

}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCPrivate *private = (GdkGCPrivate *)gc;
  GdkGCXData *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_XDATA (gc);

  if (data->clip_region)
    gdk_region_destroy (data->clip_region);

  if (rectangle)
    data->clip_region = gdk_region_rectangle (rectangle);
  else
    {
      data->clip_region = NULL;
      XSetClipMask (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc), None);
    }

  private->clip_x_origin = 0;
  private->clip_y_origin = 0;
  
  data->dirty_mask |= GDK_GC_DIRTY_CLIP;
} 

void
gdk_gc_set_clip_region (GdkGC	  *gc,
			GdkRegion *region)
{
  GdkGCPrivate *private = (GdkGCPrivate *)gc;
  GdkGCXData *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_XDATA (gc);

  if (data->clip_region)
    gdk_region_destroy (data->clip_region);

  if (region)
    data->clip_region = gdk_region_copy (region);
  else
    {
      data->clip_region = NULL;
      XSetClipMask (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc), None);
    }
  
  private->clip_x_origin = 0;
  private->clip_y_origin = 0;
  
  data->dirty_mask |= GDK_GC_DIRTY_CLIP;
}


void
gdk_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  g_return_if_fail (dst_gc != NULL);
  g_return_if_fail (src_gc != NULL);
  
  XCopyGC (GDK_GC_XDISPLAY (src_gc), GDK_GC_XGC (src_gc), ~((~1) << GCLastBit),
	   GDK_GC_XGC (dst_gc));
}
