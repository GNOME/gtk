#include "gdkprivate-nanox.h"

GdkRegion*
gdk_region_new (void)
{
  return NULL;
}


void
gdk_region_destroy (GdkRegion *region)
{
}


gboolean
gdk_region_empty (GdkRegion      *region)
{
  return 1;
}

gboolean
gdk_region_equal (GdkRegion      *region1,
                  GdkRegion      *region2)
{
  return 1;
}


void
gdk_region_get_clipbox(GdkRegion    *region,
		       GdkRectangle *rectangle)
{
}

gboolean
gdk_region_point_in (GdkRegion      *region,
                     gint           x,
		     gint           y)
{
  return 0;
}

GdkOverlapType
gdk_region_rect_in (GdkRegion      *region,
                    GdkRectangle   *rect)
{
  return 0;
}

GdkRegion *
gdk_region_polygon (GdkPoint    *points,
		    gint         npoints,
		    GdkFillRule  fill_rule)
{
  return NULL;
}

void          
gdk_region_offset (GdkRegion      *region,
                   gint           dx,
		   gint           dy)
{
}

void
gdk_region_shrink (GdkRegion      *region,
                   gint           dx,
		   gint           dy)
{
}

GdkRegion*    
gdk_region_union_with_rect (GdkRegion      *region,
                            GdkRectangle   *rect)
{
  return NULL;
}

GdkRegion*    
gdk_regions_intersect (GdkRegion      *source1,
                       GdkRegion      *source2)
{
  return NULL;
}

GdkRegion* 
gdk_regions_union (GdkRegion      *source1,
                   GdkRegion      *source2)
{
  return NULL;
}

GdkRegion*    
gdk_regions_subtract (GdkRegion      *source1,
                      GdkRegion      *source2)
{
  return NULL;
}

GdkRegion*    
gdk_regions_xor (GdkRegion      *source1,
                 GdkRegion      *source2)
{
  return NULL;
}

