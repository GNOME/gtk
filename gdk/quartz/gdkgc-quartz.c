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

#include <config.h>

#include "gdkgc.h"
#include "gdkprivate-quartz.h"

static gpointer parent_class = NULL;

static void
gdk_quartz_gc_get_values (GdkGC       *gc,
			  GdkGCValues *values)
{
  GdkGCQuartz *private;

  private = GDK_GC_QUARTZ (gc);

  memset (values, 0, sizeof (GdkGCValues));
  
  /* FIXME: Fill in more value as/if they are added. */

  values->foreground.pixel = _gdk_gc_get_fg_pixel (gc);
  values->background.pixel = _gdk_gc_get_bg_pixel (gc);

  values->function = GDK_COPY;

  values->fill = _gdk_gc_get_fill (gc);
  values->tile = _gdk_gc_get_tile (gc);
  values->stipple = _gdk_gc_get_stipple (gc);
  
  /* The X11 backend always returns a NULL clip_mask. */
  values->clip_mask = NULL;

  values->ts_x_origin = gc->ts_x_origin;
  values->ts_y_origin = gc->ts_y_origin;
  values->clip_x_origin = gc->clip_x_origin;
  values->clip_y_origin = gc->clip_y_origin;
}

static void
gdk_quartz_gc_set_values (GdkGC           *gc,
			  GdkGCValues     *values,
			  GdkGCValuesMask  mask)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);

  private->values_mask |= mask;

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
}

static void
gdk_quartz_gc_set_dashes (GdkGC *gc,
			 gint	dash_offset,
			 gint8  dash_list[],
			 gint   n)
{
  /* FIXME: Implement */
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
        (GInstanceInitFunc) NULL,
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
_gdk_windowing_gc_set_clip_region (GdkGC     *gc,
				   GdkRegion *region)
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

  dst_quartz_gc->values_mask = src_quartz_gc->values_mask;
  
  dst_quartz_gc->have_clip_region = src_quartz_gc->have_clip_region;
  dst_quartz_gc->have_clip_mask = src_quartz_gc->have_clip_mask;

  if (dst_quartz_gc->values_mask & GDK_GC_CLIP_MASK && dst_quartz_gc->clip_mask)
    {
      CGImageRelease (dst_quartz_gc->clip_mask);
      dst_quartz_gc->clip_mask = NULL;
    }
  
  if (src_quartz_gc->values_mask & GDK_GC_CLIP_MASK && src_quartz_gc->clip_mask)
    dst_quartz_gc->clip_mask =
      CGImageCreateCopy (GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (src_quartz_gc->clip_mask)->impl)->image);
}

GdkScreen *  
gdk_gc_get_screen (GdkGC *gc)
{
  return _gdk_screen;
}

void
gdk_quartz_update_context_from_gc (CGContextRef context, GdkGC *gc)
{
  GdkGCQuartz *private;

  if (gc == NULL)
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
}
