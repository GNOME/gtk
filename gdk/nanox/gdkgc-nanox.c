#include "gdkprivate-nanox.h"

static void gdk_nanox_gc_destroy    (GdkGC           *gc);
static void gdk_nanox_gc_get_values (GdkGC           *gc,
				   GdkGCValues     *values);
static void gdk_nanox_gc_set_values (GdkGC           *gc,
				   GdkGCValues     *values,
				   GdkGCValuesMask  values_mask);
static void gdk_nanox_gc_set_dashes (GdkGC           *gc,
				   gint             dash_offset,
				   gchar            dash_list[],
				   gint             n);

static GdkGCClass gdk_nanox_gc_class = {
  gdk_nanox_gc_destroy,
  gdk_nanox_gc_get_values,
  gdk_nanox_gc_set_values,
  gdk_nanox_gc_set_dashes
};

GdkGC *
_gdk_nanox_gc_new (GdkDrawable      *drawable,
		 GdkGCValues      *values,
		 GdkGCValuesMask   values_mask)
{
	
  GdkGC *gc;
  GdkGCPrivate *private;
  
  gc = gdk_gc_alloc ();
  private = (GdkGCPrivate *)gc;

  private->klass = &gdk_nanox_gc_class;
  private->klass_data = g_new (GdkGCXData, 1);

  GDK_GC_XDATA(gc)->xgc = GrNewGC();
  GrSetGCUseBackground(GDK_GC_XDATA(gc)->xgc, 0);
  GrSetGCForeground(GDK_GC_XDATA(gc)->xgc, RGB2PIXEL(0,0,0));
  GrSetGCBackground(GDK_GC_XDATA(gc)->xgc, RGB2PIXEL(0,0,0));
  g_message("created GC: %d", GDK_GC_XDATA(gc)->xgc);
  return gc;
}


static void
gdk_nanox_gc_destroy (GdkGC *gc)
{
  GrDestroyGC (GDK_GC_XGC (gc));
  g_free (GDK_GC_XDATA (gc));
}

static void
gdk_nanox_gc_get_values (GdkGC       *gc,
		       GdkGCValues *values)
{
		g_message("unimplemented %s", __FUNCTION__);
}


static void
gdk_nanox_gc_set_values (GdkGC           *gc,
		       GdkGCValues     *values,
		       GdkGCValuesMask  values_mask)
{
	if (values_mask & GDK_GC_FOREGROUND)
	  GrSetGCForeground(GDK_GC_XGC(gc), values->foreground.pixel);
	else if (values_mask & GDK_GC_BACKGROUND)
	  GrSetGCBackground(GDK_GC_XGC(gc), values->background.pixel);
	else if (values_mask & GDK_GC_FONT)
	  GrSetGCFont(GDK_GC_XGC(gc), GDK_FONT_XFONT(values->font));
}

static void
gdk_nanox_gc_set_dashes (GdkGC *gc,
		       gint   dash_offset,
		       gchar  dash_list[],
		       gint   n)
{
		g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
		//g_message("unimplemented %s", __FUNCTION__);
}

void
gdk_gc_set_clip_region (GdkGC		 *gc,
			GdkRegion	 *region)
{
		g_message("unimplemented %s", __FUNCTION__);
}


void
gdk_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  GDK_GC_XDATA(dst_gc)->xgc = GrCopyGC(GDK_GC_XGC(src_gc));
}


