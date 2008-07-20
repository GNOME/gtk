/* gdkgc-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "config.h"

#include "gdkgc.h"
#include "gdkprivate-quartz.h"

static gpointer parent_class = NULL;

static void
gdk_quartz_gc_get_values (GdkGC       *gc,
			  GdkGCValues *values)
{
  GdkGCQuartz *private;

  private = GDK_GC_QUARTZ (gc);

  values->foreground.pixel = _gdk_gc_get_fg_pixel (gc);
  values->background.pixel = _gdk_gc_get_bg_pixel (gc);

  values->font = private->font;

  values->function = private->function;

  values->fill = _gdk_gc_get_fill (gc);
  values->tile = _gdk_gc_get_tile (gc);
  values->stipple = _gdk_gc_get_stipple (gc);
  
  /* The X11 backend always returns a NULL clip_mask. */
  values->clip_mask = NULL;

  values->ts_x_origin = gc->ts_x_origin;
  values->ts_y_origin = gc->ts_y_origin;
  values->clip_x_origin = gc->clip_x_origin;
  values->clip_y_origin = gc->clip_y_origin;

  values->graphics_exposures = private->graphics_exposures;

  values->line_width = private->line_width;
  values->line_style = private->line_style;
  values->cap_style = private->cap_style;
  values->join_style = private->join_style;
}

static void
gdk_quartz_gc_set_values (GdkGC           *gc,
			  GdkGCValues     *values,
			  GdkGCValuesMask  mask)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);

  if (mask & GDK_GC_FONT)
    {
      /* FIXME: implement font */
    }

  if (mask & GDK_GC_FUNCTION)
    private->function = values->function;

  if (mask & GDK_GC_SUBWINDOW)
    private->subwindow_mode = values->subwindow_mode;

  if (mask & GDK_GC_EXPOSURES)
    private->graphics_exposures = values->graphics_exposures;

  if (mask & GDK_GC_CLIP_MASK)
    {
      private->have_clip_region = FALSE;
      private->have_clip_mask = values->clip_mask != NULL;
      if (private->clip_mask)
	CGImageRelease (private->clip_mask);

      if (values->clip_mask)
	private->clip_mask = CGImageCreateCopy (GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (values->clip_mask)->impl)->image);
      else
	private->clip_mask = NULL;
    }

  if (mask & GDK_GC_LINE_WIDTH)
    private->line_width = values->line_width;

  if (mask & GDK_GC_LINE_STYLE)
    private->line_style = values->line_style;

  if (mask & GDK_GC_CAP_STYLE)
    private->cap_style = values->cap_style;

  if (mask & GDK_GC_JOIN_STYLE)
    private->join_style = values->join_style;
}

static void
gdk_quartz_gc_set_dashes (GdkGC *gc,
			  gint   dash_offset,
			  gint8  dash_list[],
			  gint   n)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);
  gint i;

  private->dash_count = n;
  g_free (private->dash_lengths);
  private->dash_lengths = g_new (gfloat, n);
  for (i = 0; i < n; i++)
    private->dash_lengths[i] = (gfloat) dash_list[i];
  private->dash_phase = (gfloat) dash_offset;
}

static void
gdk_gc_quartz_finalize (GObject *object)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (object);

  if (private->clip_mask)
    CGImageRelease (private->clip_mask);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_gc_quartz_class_init (GdkGCQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkGCClass *gc_class = GDK_GC_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_gc_quartz_finalize;

  gc_class->get_values = gdk_quartz_gc_get_values;
  gc_class->set_values = gdk_quartz_gc_set_values;
  gc_class->set_dashes = gdk_quartz_gc_set_dashes;
}

static void
gdk_gc_quartz_init (GdkGCQuartz *gc_quartz)
{
  gc_quartz->function = GDK_COPY;
  gc_quartz->subwindow_mode = GDK_CLIP_BY_CHILDREN;
  gc_quartz->graphics_exposures = TRUE;
  gc_quartz->line_width = 0;
  gc_quartz->line_style = GDK_LINE_SOLID;
  gc_quartz->cap_style = GDK_CAP_BUTT;
  gc_quartz->join_style = GDK_JOIN_MITER;
}

GType
_gdk_gc_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkGCQuartzClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_gc_quartz_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkGCQuartz),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_gc_quartz_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_GC,
                                            "GdkGCQuartz",
                                            &object_info, 0);
    }
  
  return object_type;
}

GdkGC *
_gdk_quartz_gc_new (GdkDrawable      *drawable,
		    GdkGCValues      *values,
		    GdkGCValuesMask   values_mask)
{
  GdkGC *gc;

  gc = g_object_new (GDK_TYPE_GC_QUARTZ, NULL);

  _gdk_gc_init (gc, drawable, values, values_mask);

  gdk_quartz_gc_set_values (gc, values, values_mask);

  return gc;
}

void
_gdk_windowing_gc_set_clip_region (GdkGC           *gc,
				   const GdkRegion *region)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);

  if ((private->have_clip_region && ! region) || private->have_clip_mask)
    {
      if (private->clip_mask)
	{
	  CGImageRelease (private->clip_mask);
	  private->clip_mask = NULL;
	}
      private->have_clip_mask = FALSE;
    }

  private->have_clip_region = region != NULL;

  gc->clip_x_origin = 0;
  gc->clip_y_origin = 0;
}

void
_gdk_windowing_gc_copy (GdkGC *dst_gc,
			GdkGC *src_gc)
{
  GdkGCQuartz *dst_quartz_gc = GDK_GC_QUARTZ (dst_gc);
  GdkGCQuartz *src_quartz_gc = GDK_GC_QUARTZ (src_gc);

  if (dst_quartz_gc->font)
    gdk_font_unref (dst_quartz_gc->font);
  dst_quartz_gc->font = src_quartz_gc->font;
  if (dst_quartz_gc->font)
    gdk_font_ref (dst_quartz_gc->font);

  dst_quartz_gc->function = src_quartz_gc->function;
  dst_quartz_gc->subwindow_mode = src_quartz_gc->subwindow_mode;
  dst_quartz_gc->graphics_exposures = src_quartz_gc->graphics_exposures;

  dst_quartz_gc->have_clip_region = src_quartz_gc->have_clip_region;
  dst_quartz_gc->have_clip_mask = src_quartz_gc->have_clip_mask;

  if (dst_quartz_gc->clip_mask)
    {
      CGImageRelease (dst_quartz_gc->clip_mask);
      dst_quartz_gc->clip_mask = NULL;
    }
  
  if (src_quartz_gc->clip_mask)
    dst_quartz_gc->clip_mask =
      CGImageCreateCopy (GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (src_quartz_gc->clip_mask)->impl)->image);

  dst_quartz_gc->line_width = src_quartz_gc->line_width;
  dst_quartz_gc->line_style = src_quartz_gc->line_style;
  dst_quartz_gc->cap_style = src_quartz_gc->cap_style;
  dst_quartz_gc->join_style = src_quartz_gc->join_style;

  g_free (dst_quartz_gc->dash_lengths);
  dst_quartz_gc->dash_lengths = g_memdup (src_quartz_gc->dash_lengths,
					  sizeof (float) * src_quartz_gc->dash_count);
  dst_quartz_gc->dash_count = src_quartz_gc->dash_count;
  dst_quartz_gc->dash_phase = src_quartz_gc->dash_phase;
}

GdkScreen *  
gdk_gc_get_screen (GdkGC *gc)
{
  return _gdk_screen;
}

static void
gdk_quartz_draw_tiled_pattern (void         *info,
			       CGContextRef  context)
{
  GdkGC       *gc = GDK_GC (info);
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);
  CGImageRef   pattern_image;
  size_t       width, height;

  pattern_image = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_tile (gc))->impl)->image;

  width = CGImageGetWidth (pattern_image);
  height = CGImageGetHeight (pattern_image);

  CGContextDrawImage (context, 
		      CGRectMake (0, 0, width, height),
		      pattern_image);
}

static void
gdk_quartz_draw_stippled_pattern (void         *info,
				  CGContextRef  context)
{
  GdkGC      *gc = GDK_GC (info);
  CGImageRef  pattern_image;
  CGRect      rect;
  gfloat      r, g, b, a;

  pattern_image = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_stipple (gc))->impl)->image;
  rect = CGRectMake (0, 0,
		     CGImageGetWidth (pattern_image),
		     CGImageGetHeight (pattern_image));

  CGContextClipToMask (context, rect, pattern_image);
  _gdk_quartz_colormap_get_rgba_from_pixel (gc->colormap, 
					    _gdk_gc_get_fg_pixel (gc),
					    &r, &g, &b, &a);
  CGContextSetRGBFillColor (context, r, g, b, a);
  CGContextFillRect (context, rect);
}

static void
gdk_quartz_draw_opaque_stippled_pattern (void         *info,
					 CGContextRef  context)
{
  GdkGC      *gc = GDK_GC (info);
  CGImageRef  pattern_image;
  CGRect      rect;
  gfloat      r, g, b, a;

  pattern_image = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_stipple (gc))->impl)->image;
  rect = CGRectMake (0, 0,
		     CGImageGetWidth (pattern_image),
		     CGImageGetHeight (pattern_image));

  _gdk_quartz_colormap_get_rgba_from_pixel (gc->colormap, 
					    _gdk_gc_get_bg_pixel (gc),
					    &r, &g, &b, &a);
  CGContextSetRGBFillColor (context, r, g, b, a);
  CGContextFillRect (context, rect);

  CGContextClipToMask (context, rect, pattern_image);
  _gdk_quartz_colormap_get_rgba_from_pixel (gc->colormap, 
					    _gdk_gc_get_fg_pixel (gc),
					    &r, &g, &b, &a);
  CGContextSetRGBFillColor (context, r, g, b, a);
  CGContextFillRect (context, rect);
}

void
_gdk_quartz_gc_update_cg_context (GdkGC                      *gc,
				  GdkDrawable                *drawable,
				  CGContextRef                context,
				  GdkQuartzContextValuesMask  mask)
{
  GdkGCQuartz *private;
  guint32      fg_pixel;
  guint32      bg_pixel;

  g_return_if_fail (gc == NULL || GDK_IS_GC (gc));

  if (!gc)
    return;

  private = GDK_GC_QUARTZ (gc);

  if (private->have_clip_region)
    {
      CGRect rect;
      CGRect *cg_rects;
      GdkRectangle *rects;
      gint n_rects, i;

      gdk_region_get_rectangles (_gdk_gc_get_clip_region (gc),
				 &rects, &n_rects);

      if (n_rects == 1)
	cg_rects = &rect;
      else
	cg_rects = g_new (CGRect, n_rects);

      for (i = 0; i < n_rects; i++)
	{
	  cg_rects[i].origin.x = rects[i].x + gc->clip_x_origin;
	  cg_rects[i].origin.y = rects[i].y + gc->clip_y_origin;
	  cg_rects[i].size.width = rects[i].width;
	  cg_rects[i].size.height = rects[i].height;
	}

      CGContextClipToRects (context, cg_rects, n_rects);

      g_free (rects);
      if (cg_rects != &rect)
	g_free (cg_rects);
    }
  else if (private->have_clip_mask && private->clip_mask)
    {
      /* Note: This is 10.4 only. For lower versions, we need to transform the
       * mask into a region.
       */
      CGContextClipToMask (context,
			   CGRectMake (gc->clip_x_origin, gc->clip_y_origin,
				       CGImageGetWidth (private->clip_mask),
				       CGImageGetHeight (private->clip_mask)),
			   private->clip_mask);
    }

  fg_pixel = _gdk_gc_get_fg_pixel (gc);
  bg_pixel = _gdk_gc_get_bg_pixel (gc);

  {
    CGBlendMode blend_mode = kCGBlendModeNormal;

    switch (private->function)
      {
      case GDK_COPY:
	blend_mode = kCGBlendModeNormal;
	break;

      case GDK_INVERT:
      case GDK_XOR:
	blend_mode = kCGBlendModeExclusion;
	fg_pixel = 0xffffffff;
	bg_pixel = 0xffffffff;
	break;

      case GDK_CLEAR:
      case GDK_AND:
      case GDK_AND_REVERSE:
      case GDK_AND_INVERT:
      case GDK_NOOP:
      case GDK_OR:
      case GDK_EQUIV:
      case GDK_OR_REVERSE:
      case GDK_COPY_INVERT:
      case GDK_OR_INVERT:
      case GDK_NAND:
      case GDK_NOR:
      case GDK_SET:
	blend_mode = kCGBlendModeNormal; /* FIXME */
	break;
      }

    CGContextSetBlendMode (context, blend_mode);
  }

  /* FIXME: implement subwindow mode */

  /* FIXME: implement graphics exposures */

  if (mask & GDK_QUARTZ_CONTEXT_STROKE)
    {
      CGLineCap  line_cap  = kCGLineCapButt;
      CGLineJoin line_join = kCGLineJoinMiter;
      gfloat     r, g, b, a;

      _gdk_quartz_colormap_get_rgba_from_pixel (gc->colormap, 
						fg_pixel,
						&r, &g, &b, &a);
      CGContextSetRGBStrokeColor (context, r, g, b, a);

      CGContextSetLineWidth (context, MAX (G_MINFLOAT, private->line_width));

      switch (private->line_style)
	{
	case GDK_LINE_SOLID:
	  CGContextSetLineDash (context, 0.0, NULL, 0);
	  break;

	case GDK_LINE_DOUBLE_DASH:
	  /* FIXME: Implement; for now, fall back to GDK_LINE_ON_OFF_DASH */

	case GDK_LINE_ON_OFF_DASH:
	  CGContextSetLineDash (context, private->dash_phase,
				private->dash_lengths, private->dash_count);
	  break;
	}

      switch (private->cap_style)
        {
	case GDK_CAP_NOT_LAST:
	  /* FIXME: Implement; for now, fall back to GDK_CAP_BUTT */
	case GDK_CAP_BUTT:
	  line_cap = kCGLineCapButt;
	  break;
	case GDK_CAP_ROUND:
	  line_cap = kCGLineCapRound;
	  break;
	case GDK_CAP_PROJECTING:
	  line_cap = kCGLineCapSquare;
	  break;
	}

      CGContextSetLineCap (context, line_cap);

      switch (private->join_style)
        {
	case GDK_JOIN_MITER:
	  line_join = kCGLineJoinMiter;
	  break;
	case GDK_JOIN_ROUND:
	  line_join = kCGLineJoinRound;
	  break;
	case GDK_JOIN_BEVEL:
	  line_join = kCGLineJoinBevel;
	  break;
	}

      CGContextSetLineJoin (context, line_join);
    }

  if (mask & GDK_QUARTZ_CONTEXT_FILL)
    {
      GdkFill         fill = _gdk_gc_get_fill (gc);
      CGColorSpaceRef baseSpace;
      CGColorSpaceRef patternSpace;
      gfloat          alpha     = 1.0;
      gfloat          colors[4] = { 0.0, 0.0, 0.0, 0.0 };
      gfloat          r, g, b, a;

      if (fill == GDK_SOLID)
	{
	  _gdk_quartz_colormap_get_rgba_from_pixel (gc->colormap, 
						    fg_pixel,
						    &r, &g, &b, &a);
	  CGContextSetRGBFillColor (context, r, g, b, a);
	}
      else
	{
	  if (!private->ts_pattern)
	    {
	      CGImageRef pattern_image = NULL;
	      gfloat     width, height;
	      gboolean   is_colored = FALSE;
	      CGPatternCallbacks callbacks =  { 0, NULL, NULL };
	      CGPoint    phase;

	      switch (fill)
		{
		case GDK_TILED:
		  pattern_image = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_tile (gc))->impl)->image;
		  is_colored = TRUE;
		  callbacks.drawPattern = gdk_quartz_draw_tiled_pattern;
		  break;
		case GDK_STIPPLED:
		  pattern_image = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_stipple (gc))->impl)->image;
		  is_colored = FALSE;
		  callbacks.drawPattern = gdk_quartz_draw_stippled_pattern;
		  break;
		case GDK_OPAQUE_STIPPLED:
		  pattern_image = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_stipple (gc))->impl)->image;
		  is_colored = TRUE;
		  callbacks.drawPattern = gdk_quartz_draw_opaque_stippled_pattern;
		  break;
		default:
		  break;
		}

	      width  = CGImageGetWidth (pattern_image);
	      height = CGImageGetHeight (pattern_image);

	      phase = CGPointApplyAffineTransform (CGPointMake (gc->ts_x_origin, gc->ts_y_origin), CGContextGetCTM (context));
	      CGContextSetPatternPhase (context, CGSizeMake (phase.x, phase.y));

	      private->ts_pattern = CGPatternCreate (private,
						     CGRectMake (0, 0, width, height),
						     CGAffineTransformIdentity,
						     width, height,
						     kCGPatternTilingConstantSpacing,
						     is_colored,
						     &callbacks);
	    }

	  baseSpace = (fill == GDK_STIPPLED) ? CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB) : NULL;
	  patternSpace = CGColorSpaceCreatePattern (baseSpace);

	  CGContextSetFillColorSpace (context, patternSpace);
	  CGColorSpaceRelease (patternSpace);
	  CGColorSpaceRelease (baseSpace);

	  if (fill == GDK_STIPPLED)
	    _gdk_quartz_colormap_get_rgba_from_pixel (gc->colormap, fg_pixel,
						      &colors[0], &colors[1],
						      &colors[2], &colors[3]);

	  CGContextSetFillPattern (context, private->ts_pattern,
				   (fill == GDK_STIPPLED) ? colors : &alpha);
       }
    }

  if (mask & GDK_QUARTZ_CONTEXT_TEXT)
    {
      /* FIXME: implement text */
    }

  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
      private->is_window = TRUE;
  else
      private->is_window = FALSE;
}
