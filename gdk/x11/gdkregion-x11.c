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
#include <X11/Xutil.h>
#include "gdk.h"
#include "gdkprivate.h"




GdkRegion*
gdk_region_new (void)
{
  GdkRegionPrivate *private;
  GdkRegion *region;
  Region xregion;

  xregion = XCreateRegion();
  private = g_new (GdkRegionPrivate, 1);
  private->xregion = xregion;
  region = (GdkRegion*) private;
  region->user_data = NULL;

  return region;
}

void
gdk_region_destroy (GdkRegion *region)
{
  GdkRegionPrivate *private;

  g_return_if_fail (region != NULL);

  private = (GdkRegionPrivate *) region;
  XDestroyRegion (private->xregion);

  g_free (private);
}

gboolean
gdk_region_empty (GdkRegion      *region)
{
  GdkRegionPrivate *private;

  g_return_val_if_fail (region != NULL, 0);

  private = (GdkRegionPrivate *) region;
  
  return XEmptyRegion (private->xregion);
}

gboolean
gdk_region_equal (GdkRegion      *region1,
                  GdkRegion      *region2)
{
  GdkRegionPrivate *private1;
  GdkRegionPrivate *private2;

  g_return_val_if_fail (region1 != NULL, 0);
  g_return_val_if_fail (region2 != NULL, 0);

  private1 = (GdkRegionPrivate *) region1;
  private2 = (GdkRegionPrivate *) region2;
  
  return XEqualRegion (private1->xregion, private2->xregion);
}

gboolean
gdk_region_point_in (GdkRegion      *region,
                     gint           x,
		     gint           y)
{
  GdkRegionPrivate *private;

  g_return_val_if_fail (region != NULL, 0);

  private = (GdkRegionPrivate *) region;
  
  return XPointInRegion (private->xregion, x, y);
}

GdkOverlapType
gdk_region_rect_in (GdkRegion      *region,
                    GdkRectangle   *rect)
{
  GdkRegionPrivate *private;
  int res;

  g_return_val_if_fail (region != NULL, 0);

  private = (GdkRegionPrivate *) region;
  
  res = XRectInRegion (private->xregion, rect->x, rect->y, rect->width, rect->height);
  
  switch (res)
  {
    case RectangleIn:   return GDK_OVERLAP_RECTANGLE_IN;
    case RectangleOut:  return GDK_OVERLAP_RECTANGLE_OUT;
    case RectanglePart: return GDK_OVERLAP_RECTANGLE_PART;
  }
  
  return GDK_OVERLAP_RECTANGLE_OUT;  /*what else ? */
}
				    
GdkRegion *
gdk_region_polygon (GdkPoint    *points,
		    gint         npoints,
		    GdkFillRule  fill_rule)
{
  GdkRegionPrivate *private;
  GdkRegion *region;
  Region xregion;
  gint xfill_rule = EvenOddRule;

  g_return_val_if_fail (points != NULL, NULL);
  g_return_val_if_fail (npoints != 0, NULL); /* maybe we should check for at least three points */

  switch (fill_rule)
    {
    case GDK_EVEN_ODD_RULE:
      xfill_rule = EvenOddRule;
      break;

    case GDK_WINDING_RULE:
      xfill_rule = WindingRule;
      break;
    }

  xregion = XPolygonRegion ((XPoint *) points, npoints, xfill_rule);
  private = g_new (GdkRegionPrivate, 1);
  private->xregion = xregion;
  region = (GdkRegion *) private;
  region->user_data = NULL;

  return region;
}

void          
gdk_region_offset (GdkRegion      *region,
                   gint           dx,
		   gint           dy)
{
  GdkRegionPrivate *private;

  g_return_if_fail (region != NULL);

  private = (GdkRegionPrivate *) region;
  
  XOffsetRegion (private->xregion, dx, dy);
}

void
gdk_region_shrink (GdkRegion      *region,
                   gint           dx,
		   gint           dy)
{
  GdkRegionPrivate *private;

  g_return_if_fail (region != NULL);

  private = (GdkRegionPrivate *) region;
  
  XShrinkRegion (private->xregion, dx, dy);
}

GdkRegion*    
gdk_region_union_with_rect (GdkRegion      *region,
                            GdkRectangle   *rect)
{
  GdkRegionPrivate *private;
  GdkRegion *res;
  GdkRegionPrivate *res_private;
  XRectangle xrect;

  g_return_val_if_fail (region != NULL, NULL);

  private = (GdkRegionPrivate *) region;
  
  xrect.x = rect->x;
  xrect.y = rect->y;
  xrect.width = rect->width;
  xrect.height = rect->height;
  
  res = gdk_region_new ();
  res_private = (GdkRegionPrivate *) res;
  
  XUnionRectWithRegion (&xrect, private->xregion, res_private->xregion);
  
  return res;
}

GdkRegion*    
gdk_regions_intersect (GdkRegion      *source1,
                       GdkRegion      *source2)
{
  GdkRegionPrivate *private1;
  GdkRegionPrivate *private2;
  GdkRegion *res;
  GdkRegionPrivate *res_private;

  g_return_val_if_fail (source1 != NULL, NULL);
  g_return_val_if_fail (source2 != NULL, NULL);

  private1 = (GdkRegionPrivate *) source1;
  private2 = (GdkRegionPrivate *) source2;

  res = gdk_region_new ();
  res_private = (GdkRegionPrivate *) res;
  
  XIntersectRegion (private1->xregion, private2->xregion, res_private->xregion);
  
  return res;
}

GdkRegion* 
gdk_regions_union (GdkRegion      *source1,
                   GdkRegion      *source2)
{
  GdkRegionPrivate *private1;
  GdkRegionPrivate *private2;
  GdkRegion *res;
  GdkRegionPrivate *res_private;

  g_return_val_if_fail (source1 != NULL, NULL);
  g_return_val_if_fail (source2 != NULL, NULL);

  private1 = (GdkRegionPrivate *) source1;
  private2 = (GdkRegionPrivate *) source2;

  res = gdk_region_new ();
  res_private = (GdkRegionPrivate *) res;
  
  XUnionRegion (private1->xregion, private2->xregion, res_private->xregion);
  
  return res;
}

GdkRegion*    
gdk_regions_subtract (GdkRegion      *source1,
                      GdkRegion      *source2)
{
  GdkRegionPrivate *private1;
  GdkRegionPrivate *private2;
  GdkRegion *res;
  GdkRegionPrivate *res_private;

  g_return_val_if_fail (source1 != NULL, NULL);
  g_return_val_if_fail (source2 != NULL, NULL);

  private1 = (GdkRegionPrivate *) source1;
  private2 = (GdkRegionPrivate *) source2;

  res = gdk_region_new ();
  res_private = (GdkRegionPrivate *) res;
  
  XSubtractRegion (private1->xregion, private2->xregion, res_private->xregion);
  
  return res;
}

GdkRegion*    
gdk_regions_xor (GdkRegion      *source1,
                 GdkRegion      *source2)
{
  GdkRegionPrivate *private1;
  GdkRegionPrivate *private2;
  GdkRegion *res;
  GdkRegionPrivate *res_private;

  g_return_val_if_fail (source1 != NULL, NULL);
  g_return_val_if_fail (source2 != NULL, NULL);

  private1 = (GdkRegionPrivate *) source1;
  private2 = (GdkRegionPrivate *) source2;

  res = gdk_region_new ();
  res_private = (GdkRegionPrivate *) res;
  
  XXorRegion (private1->xregion, private2->xregion, res_private->xregion);
  
  return res;
}



