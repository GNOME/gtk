#include "gdkprivate-fb.h"
#include "gdkgc.h"
#include "gdkfb.h"
#include "gdkregion-generic.h"

typedef enum {
  GDK_GC_DIRTY_CLIP = 1 << 0,
  GDK_GC_DIRTY_TS = 1 << 1
} GdkGCDirtyValues;

static void gdk_fb_gc_destroy    (GdkGC           *gc);
static void gdk_fb_gc_get_values (GdkGC           *gc,
				   GdkGCValues     *values);
static void gdk_fb_gc_set_values (GdkGC           *gc,
				   GdkGCValues     *values,
				   GdkGCValuesMask  values_mask);
static void gdk_fb_gc_set_dashes (GdkGC          *gc,
				  gint	          dash_offset,
				  gchar           dash_list[],
				  gint            n);

static GdkGCClass gdk_fb_gc_class = {
  gdk_fb_gc_destroy,
  gdk_fb_gc_get_values,
  gdk_fb_gc_set_values,
  gdk_fb_gc_set_dashes
};

GdkGC *
_gdk_fb_gc_new (GdkDrawable      *drawable,
		GdkGCValues      *values,
		GdkGCValuesMask   values_mask)
{
  GdkGC *gc;
  GdkGCPrivate *private;
  GdkGCFBData *data;
  
  gc = gdk_gc_alloc ();
  private = (GdkGCPrivate *)gc;

  private->klass = &gdk_fb_gc_class;
  private->klass_data = data = g_new0 (GdkGCFBData, 1);
  data->values.foreground.pixel = 255;
  data->values.foreground.red = data->values.foreground.green = data->values.foreground.blue = 65535;

  gdk_fb_gc_set_values(gc, values, values_mask);

  return gc;
}

static void
gdk_fb_gc_destroy (GdkGC *gc)
{
  if (GDK_GC_FBDATA (gc)->clip_region)
    gdk_region_destroy (GDK_GC_FBDATA (gc)->clip_region);
  
  g_free (GDK_GC_FBDATA (gc));
}

static void
gdk_fb_gc_get_values (GdkGC       *gc,
		      GdkGCValues *values)
{
  *values = GDK_GC_FBDATA(gc)->values;
}


static void
gdk_fb_gc_set_values (GdkGC           *gc,
		      GdkGCValues     *values,
		      GdkGCValuesMask  values_mask)
{
  GdkPixmap *oldpm;
  GdkFont *oldf;

  if(values_mask & GDK_GC_FOREGROUND)
    {
      GDK_GC_FBDATA(gc)->values.foreground = values->foreground;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_FOREGROUND;
    }

  if(values_mask & GDK_GC_BACKGROUND)
    {
      GDK_GC_FBDATA(gc)->values.background = values->background;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_BACKGROUND;
    }

  if(values_mask & GDK_GC_FONT)
    {
      oldf = GDK_GC_FBDATA(gc)->values.font;
      GDK_GC_FBDATA(gc)->values.font = gdk_font_ref(values->font);
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_FONT;
      if(oldf)
	gdk_font_unref(oldf);
    }

  if(values_mask & GDK_GC_FUNCTION)
    {
      GDK_GC_FBDATA(gc)->values.function = values->function;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_FUNCTION;
    }

  if(values_mask & GDK_GC_FILL)
    {
      GDK_GC_FBDATA(gc)->values.fill = values->fill;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_FILL;
    }

  if(values_mask & GDK_GC_TILE)
    {
      oldpm = GDK_GC_FBDATA(gc)->values.tile;
      if(values->tile)
	g_assert(GDK_DRAWABLE_P(values->tile)->depth >= 8);

      GDK_GC_FBDATA(gc)->values.tile = values->tile?gdk_pixmap_ref(values->tile):NULL;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_TILE;
      if(oldpm)
	gdk_pixmap_unref(oldpm);
    }

  if(values_mask & GDK_GC_STIPPLE)
    {
      oldpm = GDK_GC_FBDATA(gc)->values.stipple;
      if(values->stipple)
	g_assert(GDK_DRAWABLE_P(values->stipple)->depth == 1);
      GDK_GC_FBDATA(gc)->values.stipple = values->stipple?gdk_pixmap_ref(values->stipple):NULL;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_STIPPLE;
      if(oldpm)
	gdk_pixmap_unref(oldpm);
    }

  if(values_mask & GDK_GC_CLIP_MASK)
    {
      oldpm = GDK_GC_FBDATA(gc)->values.clip_mask;

      GDK_GC_FBDATA(gc)->values.clip_mask = values->clip_mask?gdk_pixmap_ref(values->clip_mask):NULL;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_CLIP_MASK;
      if(oldpm)
	gdk_pixmap_unref(oldpm);

      if(GDK_GC_FBDATA(gc)->clip_region)
	{
	  gdk_region_destroy(GDK_GC_FBDATA(gc)->clip_region);
	  GDK_GC_FBDATA(gc)->clip_region = NULL;
	}
    }

  if(values_mask & GDK_GC_SUBWINDOW)
    {
      GDK_GC_FBDATA(gc)->values.subwindow_mode = values->subwindow_mode;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_SUBWINDOW;
    }

  if(values_mask & GDK_GC_TS_X_ORIGIN)
    {
      GDK_GC_FBDATA(gc)->values.ts_x_origin = values->ts_x_origin;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_TS_X_ORIGIN;
    }

  if(values_mask & GDK_GC_TS_Y_ORIGIN)
    {
      GDK_GC_FBDATA(gc)->values.ts_y_origin = values->ts_y_origin;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_TS_Y_ORIGIN;
    }

  if(values_mask & GDK_GC_CLIP_X_ORIGIN)
    {
      GDK_GC_FBDATA(gc)->values.clip_x_origin = GDK_GC_P(gc)->clip_x_origin = values->clip_x_origin;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_CLIP_X_ORIGIN;
    }

  if(values_mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      GDK_GC_FBDATA(gc)->values.clip_y_origin = GDK_GC_P(gc)->clip_y_origin = values->clip_y_origin;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_CLIP_Y_ORIGIN;
    }

  if(values_mask & GDK_GC_EXPOSURES)
    {
      GDK_GC_FBDATA(gc)->values.graphics_exposures = values->graphics_exposures;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_EXPOSURES;
    }

  if(values_mask & GDK_GC_LINE_WIDTH)
    {
      GDK_GC_FBDATA(gc)->values.line_width = values->line_width;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_LINE_WIDTH;
    }

  if(values_mask & GDK_GC_LINE_STYLE)
    {
      GDK_GC_FBDATA(gc)->values.line_style = values->line_style;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_LINE_STYLE;
    }

  if(values_mask & GDK_GC_CAP_STYLE)
    {
      GDK_GC_FBDATA(gc)->values.cap_style = values->cap_style;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_CAP_STYLE;
    }

  if(values_mask & GDK_GC_JOIN_STYLE)
    {
      GDK_GC_FBDATA(gc)->values.join_style = values->join_style;
      GDK_GC_FBDATA(gc)->values_mask |= GDK_GC_JOIN_STYLE;
    }
}

static void
gdk_fb_gc_set_dashes (GdkGC *gc,
		      gint dash_offset,
		      gchar dash_list[],
		      gint n)
{
  GDK_GC_FBDATA(gc)->dash_offset = dash_offset;
  GDK_GC_FBDATA(gc)->dash_list_len = n;
  if(n)
    {
      GDK_GC_FBDATA(gc)->dash_list = g_realloc(GDK_GC_FBDATA(gc)->dash_list, n);
      memcpy(GDK_GC_FBDATA(gc)->dash_list, dash_list, n);
    }
  else
    {
      g_free(GDK_GC_FBDATA(gc)->dash_list);
      GDK_GC_FBDATA(gc)->dash_list = NULL;
    }
}

static void
gc_unset_cmask(GdkGC *gc)
{
  GdkGCPrivate *private = (GdkGCPrivate *)gc;
  GdkGCFBData *data;
  data = GDK_GC_FBDATA (gc);
  if(data->values.clip_mask)
    {
      gdk_pixmap_unref(data->values.clip_mask);
      data->values.clip_mask = NULL;
      data->values_mask &= ~ GDK_GC_CLIP_MASK;
    }
}
void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCPrivate *private = (GdkGCPrivate *)gc;
  GdkGCFBData *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_FBDATA (gc);

  if (data->clip_region)
    {
      gdk_region_destroy (data->clip_region);
      data->clip_region = NULL;
    }

  if (rectangle)
    data->clip_region = gdk_region_rectangle (rectangle);

  private->clip_x_origin = 0;
  private->clip_y_origin = 0;
  data->values.clip_x_origin = 0;
  data->values.clip_y_origin = 0;

  gc_unset_cmask(gc);
} 

void
gdk_gc_set_clip_region (GdkGC	  *gc,
			GdkRegion *region)
{
  GdkGCPrivate *private = (GdkGCPrivate *)gc;
  GdkGCFBData *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_FBDATA (gc);

  if(region == data->clip_region)
    return;

  if (data->clip_region)
    {
      gdk_region_destroy (data->clip_region);
      data->clip_region = NULL;
    }

  if (region)
    data->clip_region = gdk_region_copy (region);
  
  private->clip_x_origin = 0;
  private->clip_y_origin = 0;
  data->values.clip_x_origin = 0;
  data->values.clip_y_origin = 0;

  gc_unset_cmask(gc);
}


void
gdk_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  g_return_if_fail (dst_gc != NULL);
  g_return_if_fail (src_gc != NULL);

  if(GDK_GC_FBDATA(dst_gc)->clip_region)
    gdk_region_destroy(GDK_GC_FBDATA(dst_gc)->clip_region);

  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_FONT)
    gdk_font_unref(GDK_GC_FBDATA(dst_gc)->values.font);
  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_TILE)
    gdk_pixmap_unref(GDK_GC_FBDATA(dst_gc)->values.tile);
  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_STIPPLE)
    gdk_pixmap_unref(GDK_GC_FBDATA(dst_gc)->values.stipple);
  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_CLIP_MASK)
    gdk_pixmap_unref(GDK_GC_FBDATA(dst_gc)->values.clip_mask);

  *dst_gc = *src_gc;
  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_FONT)
    gdk_font_ref(GDK_GC_FBDATA(dst_gc)->values.font);
  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_TILE)
    gdk_pixmap_ref(GDK_GC_FBDATA(dst_gc)->values.tile);
  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_STIPPLE)
    gdk_pixmap_ref(GDK_GC_FBDATA(dst_gc)->values.stipple);
  if(GDK_GC_FBDATA(dst_gc)->values_mask & GDK_GC_CLIP_MASK)
    gdk_pixmap_ref(GDK_GC_FBDATA(dst_gc)->values.clip_mask);
  if(GDK_GC_FBDATA(dst_gc)->clip_region)
    GDK_GC_FBDATA(dst_gc)->clip_region = gdk_region_copy(GDK_GC_FBDATA(dst_gc)->clip_region);
}
