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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <string.h>
#include <X11/Xlib.h>
#include "gdk.h"
#include "gdkprivate.h"


GdkGC*
gdk_gc_new (GdkWindow *window)
{
  return gdk_gc_new_with_values (window, NULL, 0);
}

GdkGC*
gdk_gc_new_with_values (GdkWindow	*window,
			GdkGCValues	*values,
			GdkGCValuesMask	 values_mask)
{
  GdkWindowPrivate *window_private;
  GdkGC *gc;
  GdkGCPrivate *private;
  Window xwindow;
  XGCValues xvalues;
  unsigned long xvalues_mask;

  g_return_val_if_fail (window != NULL, NULL);

  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return NULL;

  private = g_new (GdkGCPrivate, 1);
  gc = (GdkGC*) private;

  xwindow = window_private->xwindow;
  private->xdisplay = window_private->xdisplay;
  private->ref_count = 1;

  xvalues.function = GXcopy;
  xvalues.fill_style = FillSolid;
  xvalues.arc_mode = ArcPieSlice;
  xvalues.subwindow_mode = ClipByChildren;
  xvalues.graphics_exposures = True;
  xvalues_mask = GCFunction | GCFillStyle | GCArcMode | GCSubwindowMode | GCGraphicsExposures;

  if (values_mask & GDK_GC_FOREGROUND)
    {
      xvalues.foreground = values->foreground.pixel;
      xvalues_mask |= GCForeground;
    }
  if (values_mask & GDK_GC_BACKGROUND)
    {
      xvalues.background = values->background.pixel;
      xvalues_mask |= GCBackground;
    }
  if ((values_mask & GDK_GC_FONT) && (values->font->type == GDK_FONT_FONT))
    {
      xvalues.font = ((XFontStruct *) ((GdkFontPrivate*) values->font)->xfont)->fid;
      xvalues_mask |= GCFont;
    }
  if (values_mask & GDK_GC_FUNCTION)
    {
      switch (values->function)
	{
	case GDK_COPY:
	  xvalues.function = GXcopy;
	  break;
	case GDK_INVERT:
	  xvalues.function = GXinvert;
	  break;
	case GDK_XOR:
	  xvalues.function = GXxor;
	  break;
	}
      xvalues_mask |= GCFunction;
    }
  if (values_mask & GDK_GC_FILL)
    {
      switch (values->fill)
	{
	case GDK_SOLID:
	  xvalues.fill_style = FillSolid;
	  break;
	case GDK_TILED:
	  xvalues.fill_style = FillTiled;
	  break;
	case GDK_STIPPLED:
	  xvalues.fill_style = FillStippled;
	  break;
	case GDK_OPAQUE_STIPPLED:
	  xvalues.fill_style = FillOpaqueStippled;
	  break;
	}
      xvalues_mask |= GCFillStyle;
    }
  if (values_mask & GDK_GC_TILE)
    {
      xvalues.tile = ((GdkPixmapPrivate*) values->tile)->xwindow;
      xvalues_mask |= GCTile;
    }
  if (values_mask & GDK_GC_STIPPLE)
    {
      xvalues.stipple = ((GdkPixmapPrivate*) values->stipple)->xwindow;
      xvalues_mask |= GCStipple;
    }
  if (values_mask & GDK_GC_CLIP_MASK)
    {
      xvalues.clip_mask = ((GdkPixmapPrivate*) values->clip_mask)->xwindow;
      xvalues_mask |= GCClipMask;
    }
  if (values_mask & GDK_GC_SUBWINDOW)
    {
      xvalues.subwindow_mode = values->subwindow_mode;
      xvalues_mask |= GCSubwindowMode;
    }
  if (values_mask & GDK_GC_TS_X_ORIGIN)
    {
      xvalues.ts_x_origin = values->ts_x_origin;
      xvalues_mask |= GCTileStipXOrigin;
    }
  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    {
      xvalues.ts_y_origin = values->ts_y_origin;
      xvalues_mask |= GCTileStipYOrigin;
    }
  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    {
      xvalues.clip_x_origin = values->clip_x_origin;
      xvalues_mask |= GCClipXOrigin;
    }
  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      xvalues.clip_y_origin = values->clip_y_origin;
      xvalues_mask |= GCClipYOrigin;
    }
 
  if (values_mask & GDK_GC_EXPOSURES)
    xvalues.graphics_exposures = values->graphics_exposures;
  else
    xvalues.graphics_exposures = False;
  xvalues_mask |= GCGraphicsExposures;

  if (values_mask & GDK_GC_LINE_WIDTH)
    {
      xvalues.line_width = values->line_width;
      xvalues_mask |= GCLineWidth;
    }
  if (values_mask & GDK_GC_LINE_STYLE)
    {
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
	  xvalues.line_style = LineSolid;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	  xvalues.line_style = LineOnOffDash;
	  break;
	case GDK_LINE_DOUBLE_DASH:
	  xvalues.line_style = LineDoubleDash;
	  break;
	}
      xvalues_mask |= GCLineStyle;
    }
  if (values_mask & GDK_GC_CAP_STYLE)
    {
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:
	  xvalues.cap_style = CapNotLast;
	  break;
	case GDK_CAP_BUTT:
	  xvalues.cap_style = CapButt;
	  break;
	case GDK_CAP_ROUND:
	  xvalues.cap_style = CapRound;
	  break;
	case GDK_CAP_PROJECTING:
	  xvalues.cap_style = CapProjecting;
	  break;
	}
      xvalues_mask |= GCCapStyle;
    }
  if (values_mask & GDK_GC_JOIN_STYLE)
    {
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  xvalues.join_style = JoinMiter;
	  break;
	case GDK_JOIN_ROUND:
	  xvalues.join_style = JoinRound;
	  break;
	case GDK_JOIN_BEVEL:
	  xvalues.join_style = JoinBevel;
	  break;
	}
      xvalues_mask |= GCJoinStyle;
    }

  private->xgc = XCreateGC (private->xdisplay, xwindow, xvalues_mask, &xvalues);

  return gc;
}

void
gdk_gc_destroy (GdkGC *gc)
{
  gdk_gc_unref (gc);
}

GdkGC *
gdk_gc_ref (GdkGC *gc)
{
  GdkGCPrivate *private = (GdkGCPrivate*) gc;

  g_return_val_if_fail (gc != NULL, NULL);
  private->ref_count += 1;

  return gc;
}

void
gdk_gc_unref (GdkGC *gc)
{
  GdkGCPrivate *private = (GdkGCPrivate*) gc;
  
  g_return_if_fail (gc != NULL);
  
  if (private->ref_count > 1)
    private->ref_count -= 1;
  else
    {
      XFreeGC (private->xdisplay, private->xgc);
      memset (gc, 0, sizeof (GdkGCPrivate));
      g_free (gc);
    }
}

void
gdk_gc_get_values (GdkGC       *gc,
		   GdkGCValues *values)
{
  GdkGCPrivate *private;
  XGCValues xvalues;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (values != NULL);

  private = (GdkGCPrivate*) gc;

  if (XGetGCValues (private->xdisplay, private->xgc,
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

void
gdk_gc_set_foreground (GdkGC	*gc,
		       GdkColor *color)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (color != NULL);

  private = (GdkGCPrivate*) gc;
  XSetForeground (private->xdisplay, private->xgc, color->pixel);
}

void
gdk_gc_set_background (GdkGC	*gc,
		       GdkColor *color)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (color != NULL);

  private = (GdkGCPrivate*) gc;
  XSetBackground (private->xdisplay, private->xgc, color->pixel);
}

void
gdk_gc_set_font (GdkGC	 *gc,
		 GdkFont *font)
{
  GdkGCPrivate *gc_private;
  GdkFontPrivate *font_private;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (font != NULL);

  if (font->type == GDK_FONT_FONT)
    {
      gc_private = (GdkGCPrivate*) gc;
      font_private = (GdkFontPrivate*) font;
      
      XSetFont (gc_private->xdisplay, gc_private->xgc,
		((XFontStruct *) font_private->xfont)->fid);
    }
}

void
gdk_gc_set_function (GdkGC	 *gc,
		     GdkFunction  function)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  switch (function)
    {
    case GDK_COPY:
      XSetFunction (private->xdisplay, private->xgc, GXcopy);
      break;
    case GDK_INVERT:
      XSetFunction (private->xdisplay, private->xgc, GXinvert);
      break;
    case GDK_XOR:
      XSetFunction (private->xdisplay, private->xgc, GXxor);
      break;
    }
}

void
gdk_gc_set_fill (GdkGC	 *gc,
		 GdkFill  fill)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  switch (fill)
    {
    case GDK_SOLID:
      XSetFillStyle (private->xdisplay, private->xgc, FillSolid);
      break;
    case GDK_TILED:
      XSetFillStyle (private->xdisplay, private->xgc, FillTiled);
      break;
    case GDK_STIPPLED:
      XSetFillStyle (private->xdisplay, private->xgc, FillStippled);
      break;
    case GDK_OPAQUE_STIPPLED:
      XSetFillStyle (private->xdisplay, private->xgc, FillOpaqueStippled);
      break;
    }
}

void
gdk_gc_set_tile (GdkGC	   *gc,
		 GdkPixmap *tile)
{
  GdkGCPrivate *private;
  GdkPixmapPrivate *pixmap_private;
  Pixmap pixmap;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  pixmap = None;
  if (tile)
    {
      pixmap_private = (GdkPixmapPrivate*) tile;
      pixmap = pixmap_private->xwindow;
    }

  XSetTile (private->xdisplay, private->xgc, pixmap);
}

void
gdk_gc_set_stipple (GdkGC     *gc,
		    GdkPixmap *stipple)
{
  GdkGCPrivate *private;
  GdkPixmapPrivate *pixmap_private;
  Pixmap pixmap;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  pixmap = None;
  if (stipple)
    {
      pixmap_private = (GdkPixmapPrivate*) stipple;
      pixmap = pixmap_private->xwindow;
    }

  XSetStipple (private->xdisplay, private->xgc, pixmap);
}

void
gdk_gc_set_ts_origin (GdkGC *gc,
		      gint   x,
		      gint   y)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  XSetTSOrigin (private->xdisplay, private->xgc, x, y);
}

void
gdk_gc_set_clip_origin (GdkGC *gc,
			gint   x,
			gint   y)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  XSetClipOrigin (private->xdisplay, private->xgc, x, y);
}

void
gdk_gc_set_clip_mask (GdkGC	*gc,
		      GdkBitmap *mask)
{
  GdkGCPrivate *private;
  Pixmap xmask;
  
  g_return_if_fail (gc != NULL);
  
  if (mask)
    {
      GdkWindowPrivate *mask_private;
      
      mask_private = (GdkWindowPrivate*) mask;
      if (mask_private->destroyed)
	return;
      xmask = mask_private->xwindow;
    }
  else
    xmask = None;
  
  private = (GdkGCPrivate*) gc;

  XSetClipMask (private->xdisplay, private->xgc, xmask);
}


void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCPrivate *private;
  XRectangle xrectangle;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  if (rectangle)
    {
      xrectangle.x = rectangle->x; 
      xrectangle.y = rectangle->y;
      xrectangle.width = rectangle->width;
      xrectangle.height = rectangle->height;
      
      XSetClipRectangles (private->xdisplay, private->xgc, 0, 0,
			  &xrectangle, 1, Unsorted);
    }
  else
    XSetClipMask (private->xdisplay, private->xgc, None);
} 

void
gdk_gc_set_clip_region (GdkGC		 *gc,
			GdkRegion	 *region)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  if (region)
    {
      GdkRegionPrivate *region_private;

      region_private = (GdkRegionPrivate*) region;
      XSetRegion (private->xdisplay, private->xgc, region_private->xregion);
    }
  else
    XSetClipMask (private->xdisplay, private->xgc, None);
}

void
gdk_gc_set_subwindow (GdkGC	       *gc,
		      GdkSubwindowMode	mode)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  XSetSubwindowMode (private->xdisplay, private->xgc, mode);
}

void
gdk_gc_set_exposures (GdkGC *gc,
		      gint   exposures)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  XSetGraphicsExposures (private->xdisplay, private->xgc, exposures);
}

void
gdk_gc_set_line_attributes (GdkGC	*gc,
			    gint	 line_width,
			    GdkLineStyle line_style,
			    GdkCapStyle	 cap_style,
			    GdkJoinStyle join_style)
{
  GdkGCPrivate *private;
  int xline_style;
  int xcap_style;
  int xjoin_style;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  switch (line_style)
    {
    case GDK_LINE_SOLID:
      xline_style = LineSolid;
      break;
    case GDK_LINE_ON_OFF_DASH:
      xline_style = LineOnOffDash;
      break;
    case GDK_LINE_DOUBLE_DASH:
      xline_style = LineDoubleDash;
      break;
    default:
      xline_style = None;
    }

  switch (cap_style)
    {
    case GDK_CAP_NOT_LAST:
      xcap_style = CapNotLast;
      break;
    case GDK_CAP_BUTT:
      xcap_style = CapButt;
      break;
    case GDK_CAP_ROUND:
      xcap_style = CapRound;
      break;
    case GDK_CAP_PROJECTING:
      xcap_style = CapProjecting;
      break;
    default:
      xcap_style = None;
    }

  switch (join_style)
    {
    case GDK_JOIN_MITER:
      xjoin_style = JoinMiter;
      break;
    case GDK_JOIN_ROUND:
      xjoin_style = JoinRound;
      break;
    case GDK_JOIN_BEVEL:
      xjoin_style = JoinBevel;
      break;
    default:
      xjoin_style = None;
    }

  XSetLineAttributes (private->xdisplay, private->xgc, line_width,
		      xline_style, xcap_style, xjoin_style);
}

void
gdk_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  GdkGCPrivate *dst_private, *src_private;

  src_private = (GdkGCPrivate *) src_gc;
  dst_private = (GdkGCPrivate *) dst_gc;

  XCopyGC (src_private->xdisplay, src_private->xgc, ~((~1) << GCLastBit),
	   dst_private->xgc);
}
