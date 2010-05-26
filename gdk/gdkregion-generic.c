#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gdkregion.h>
#include "gdkalias.h"

/**
 * gdk_region_new:
 *
 * Creates a new empty #GdkRegion.
 *
 * Returns: a new empty #GdkRegion
 */
GdkRegion *
gdk_region_new (void)
{
  return cairo_region_create ();
}

/**
 * gdk_region_rectangle:
 * @rectangle: a #GdkRectangle
 * 
 * Creates a new region containing the area @rectangle.
 * 
 * Return value: a new region
 **/
GdkRegion *
gdk_region_rectangle (const GdkRectangle *rectangle)
{
  g_return_val_if_fail (rectangle != NULL, NULL);

  if (rectangle->width <= 0 || rectangle->height <= 0)
    return gdk_region_new();

  return cairo_region_create_rectangle ((cairo_rectangle_int_t *) rectangle);
}

/**
 * gdk_region_copy:
 * @region: a #GdkRegion
 * 
 * Copies @region, creating an identical new region.
 * 
 * Return value: a new region identical to @region
 **/
GdkRegion *
gdk_region_copy (const GdkRegion *region)
{
  return cairo_region_copy (region);
}

/**
 * gdk_region_get_clipbox:
 * @region: a #GdkRegion
 * @rectangle: return location for the clipbox
 *
 * Obtains the smallest rectangle which includes the entire #GdkRegion.
 *
 */
void
gdk_region_get_clipbox (const GdkRegion *region,
			GdkRectangle    *rectangle)
{
  g_return_if_fail (region != NULL);
  g_return_if_fail (rectangle != NULL);
  
  cairo_region_get_extents (region, (cairo_rectangle_int_t *) rectangle);
}


/**
 * gdk_region_get_rectangles:
 * @region: a #GdkRegion
 * @rectangles: (array length=n_rectangles) (transfer container): return location for an array of rectangles
 * @n_rectangles: length of returned array
 *
 * Obtains the area covered by the region as a list of rectangles.
 * The array returned in @rectangles must be freed with g_free().
 **/
void
gdk_region_get_rectangles (const GdkRegion  *region,
                           GdkRectangle    **rectangles,
                           gint             *n_rectangles)
{
  gint i, n;
  GdkRectangle *rects;
  
  g_return_if_fail (region != NULL);
  g_return_if_fail (rectangles != NULL);
  g_return_if_fail (n_rectangles != NULL);
  
  n = cairo_region_num_rectangles (region);
  rects = g_new (GdkRectangle, n);

  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (region, i, (cairo_rectangle_int_t *) &rects[i]);
    }

  *n_rectangles = n;
  *rectangles = rects;
}

/**
 * gdk_region_union_with_rect:
 * @region: a #GdkRegion.
 * @rect: a #GdkRectangle.
 * 
 * Sets the area of @region to the union of the areas of @region and
 * @rect. The resulting area is the set of pixels contained in
 * either @region or @rect.
 **/
void
gdk_region_union_with_rect (GdkRegion          *region,
			    const GdkRectangle *rect)
{
  g_return_if_fail (region != NULL);
  g_return_if_fail (rect != NULL);

  if (rect->width <= 0 || rect->height <= 0)
    return;
    
  cairo_region_union_rectangle (region, (cairo_rectangle_int_t *) rect);
}

/**
 * gdk_region_destroy:
 * @region: a #GdkRegion
 *
 * Destroys a #GdkRegion.
 */
void
gdk_region_destroy (GdkRegion *region)
{
  g_return_if_fail (region != NULL);

  cairo_region_destroy (region);
}


/**
 * gdk_region_offset:
 * @region: a #GdkRegion
 * @dx: the distance to move the region horizontally
 * @dy: the distance to move the region vertically
 *
 * Moves a region the specified distance.
 */
void
gdk_region_offset (GdkRegion *region,
		   gint       x,
		   gint       y)
{
  cairo_region_translate (region, x, y);
}

/**
 * gdk_region_shrink:
 * @region: a #GdkRegion
 * @dx: the number of pixels to shrink the region horizontally
 * @dy: the number of pixels to shrink the region vertically
 *
 * Resizes a region by the specified amount.
 * Positive values shrink the region. Negative values expand it.
 */
void
gdk_region_shrink (GdkRegion *region,
		   int        dx,
		   int        dy)
{
  GdkRectangle *rects;
  int i, n_rects;

  gdk_region_get_rectangles (region, &rects, &n_rects);
  /* clear region */
  gdk_region_subtract (region, region);

  for (i = 0; i < n_rects; i++)
    {
      if (rects[i].width <= 2 * dx ||
          rects[i].height <= 2 * dy)
        continue;

      rects[i].x += dx;
      rects[i].y += dy;
      rects[i].width -= 2 * dx;
      rects[i].height -= 2 * dy;
      cairo_region_union_rectangle (region, (cairo_rectangle_int_t *) &rects[i]);
    }
}

/**
 * gdk_region_intersect:
 * @source1: a #GdkRegion
 * @source2: another #GdkRegion
 *
 * Sets the area of @source1 to the intersection of the areas of @source1
 * and @source2. The resulting area is the set of pixels contained in
 * both @source1 and @source2.
 **/
void
gdk_region_intersect (GdkRegion       *source1,
		      const GdkRegion *source2)
{
  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);
  
  cairo_region_intersect (source1, (cairo_region_t *) source2);
}

/**
 * gdk_region_union:
 * @source1:  a #GdkRegion
 * @source2: a #GdkRegion 
 * 
 * Sets the area of @source1 to the union of the areas of @source1 and
 * @source2. The resulting area is the set of pixels contained in
 * either @source1 or @source2.
 **/
void
gdk_region_union (GdkRegion       *source1,
		  const GdkRegion *source2)
{
  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);
  
  cairo_region_union (source1, (cairo_region_t *) source2);
}

/**
 * gdk_region_subtract:
 * @source1: a #GdkRegion
 * @source2: another #GdkRegion
 *
 * Subtracts the area of @source2 from the area @source1. The resulting
 * area is the set of pixels contained in @source1 but not in @source2.
 **/
void
gdk_region_subtract (GdkRegion       *source1,
		     const GdkRegion *source2)
{
  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);
  
  cairo_region_subtract (source1, source2);
}

/**
 * gdk_region_xor:
 * @source1: a #GdkRegion
 * @source2: another #GdkRegion
 *
 * Sets the area of @source1 to the exclusive-OR of the areas of @source1
 * and @source2. The resulting area is the set of pixels contained in one
 * or the other of the two sources but not in both.
 **/
void
gdk_region_xor (GdkRegion       *source1,
		const GdkRegion *source2)
{
  GdkRegion *trb;

  g_return_if_fail (source1 != NULL);
  g_return_if_fail (source2 != NULL);

  trb = gdk_region_copy (source2);

  gdk_region_subtract (trb, source1);
  gdk_region_subtract (source1, source2);

  gdk_region_union (source1, trb);
  
  gdk_region_destroy (trb);
}

/**
 * gdk_region_empty: 
 * @region: a #GdkRegion
 *
 * Finds out if the #GdkRegion is empty.
 *
 * Returns: %TRUE if @region is empty.
 */
gboolean
gdk_region_empty (const GdkRegion *region)
{
  g_return_val_if_fail (region != NULL, FALSE);
  
  return cairo_region_is_empty (region);
}

/**
 * gdk_region_equal:
 * @region1: a #GdkRegion
 * @region2: a #GdkRegion
 *
 * Finds out if the two regions are the same.
 *
 * Returns: %TRUE if @region1 and @region2 are equal.
 */
gboolean
gdk_region_equal (const GdkRegion *region1,
		  const GdkRegion *region2)
{
  g_return_val_if_fail (region1 != NULL, FALSE);
  g_return_val_if_fail (region2 != NULL, FALSE);

  return cairo_region_equal (region1, region2);
}

/**
 * gdk_region_rect_equal:
 * @region: a #GdkRegion
 * @rectangle: a #GdkRectangle
 *
 * Finds out if a regions is the same as a rectangle.
 *
 * Returns: %TRUE if @region and @rectangle are equal.
 *
 * Since: 2.18
 */
gboolean
gdk_region_rect_equal (const GdkRegion    *region,
		       const GdkRectangle *rectangle)
{
  cairo_rectangle_int_t extents;

  g_return_val_if_fail (region != NULL, FALSE);
  g_return_val_if_fail (rectangle != NULL, FALSE);

  if (cairo_region_num_rectangles (region) != 1) return FALSE;
  cairo_region_get_extents (region, &extents);
  if (extents.x != rectangle->x) return FALSE;
  else if (extents.y != rectangle->y) return FALSE;
  else if (extents.width != rectangle->width) return FALSE;
  else if (extents.height != rectangle->height) return FALSE;
  return TRUE;
}

/**
 * gdk_region_point_in:
 * @region: a #GdkRegion
 * @x: the x coordinate of a point
 * @y: the y coordinate of a point
 *
 * Finds out if a point is in a region.
 *
 * Returns: %TRUE if the point is in @region.
 */
gboolean
gdk_region_point_in (const GdkRegion *region,
		     int              x,
		     int              y)
{
  g_return_val_if_fail (region != NULL, FALSE);

  return cairo_region_contains_point (region, x, y);
}

/**
 * gdk_region_rect_in: 
 * @region: a #GdkRegion.
 * @rectangle: a #GdkRectangle.
 *
 * Tests whether a rectangle is within a region.
 *
 * Returns: %GDK_OVERLAP_RECTANGLE_IN, %GDK_OVERLAP_RECTANGLE_OUT, or
 *   %GDK_OVERLAP_RECTANGLE_PART, depending on whether the rectangle is inside,
 *   outside, or partly inside the #GdkRegion, respectively.
 */
GdkOverlapType
gdk_region_rect_in (const GdkRegion    *region,
		    const GdkRectangle *rectangle)
{
  g_return_val_if_fail (region != NULL, GDK_OVERLAP_RECTANGLE_OUT);
  g_return_val_if_fail (rectangle != NULL, GDK_OVERLAP_RECTANGLE_OUT);

  return cairo_region_contains_rectangle (region, (cairo_rectangle_int_t *) rectangle);
}


static void
gdk_region_unsorted_spans_intersect_foreach (GdkRegion     *region,
					     const GdkSpan *spans,
					     int            n_spans,
					     GdkSpanFunc    function,
					     gpointer       data)
{
  gint i, j, left, right, y, n_rects;
  gint clipped_left, clipped_right;
  cairo_rectangle_int_t pbox, extents;

  n_rects = cairo_region_num_rectangles (region);
  if (!n_rects)
    return;

  cairo_region_get_extents (region, &extents);
  for (i=0;i<n_spans;i++)
    {
      y = spans[i].y;
      left = spans[i].x;
      right = left + spans[i].width; /* right is not in the span! */
    
      if (! ((extents.y <= y) &&
	     (extents.y + extents.height > y) &&
	     (extents.x < right) &&
	     (extents.x + extents.width > left)) ) 
	continue;

      /* can stop when we passed y */
      for (j = 0; j < n_rects; j++)
	{
          cairo_region_get_rectangle (region, j, &pbox);

	  if (pbox.y + pbox.height <= y)
	    continue; /* Not quite there yet */
	  
	  if (pbox.y > y)
	    break; /* passed the spanline */
	  
	  if ((right > pbox.x) && (left < pbox.x + pbox.width)) 
	    {
              GdkSpan out_span;

	      clipped_left = MAX (left, pbox.x);
	      clipped_right = MIN (right, pbox.x + pbox.width);
	      
	      out_span.y = y;
	      out_span.x = clipped_left;
	      out_span.width = clipped_right - clipped_left;
	      (*function) (&out_span, data);
	    }
	}
    }
}

/**
 * gdk_region_spans_intersect_foreach:
 * @region: a #GdkRegion
 * @spans: an array of #GdkSpans
 * @n_spans: the length of @spans
 * @sorted: %TRUE if @spans is sorted wrt. the y coordinate
 * @function: function to call on each span in the intersection
 * @data: data to pass to @function
 *
 * Calls a function on each span in the intersection of @region and @spans.
 */
void
gdk_region_spans_intersect_foreach (GdkRegion     *region,
				    const GdkSpan *spans,
				    int            n_spans,
				    gboolean       sorted,
				    GdkSpanFunc    function,
				    gpointer       data)
{
  gint i, left, right, y, n_rects;
  gint clipped_left, clipped_right;
  GdkRectangle pbox;
  const GdkSpan *span, *tmpspan;
  const GdkSpan *end_span;

  g_return_if_fail (region != NULL);
  g_return_if_fail (spans != NULL);

  if (!sorted)
    {
      gdk_region_unsorted_spans_intersect_foreach (region,
						   spans,
						   n_spans,
						   function,
						   data);
      return;
    }
  
  n_rects = cairo_region_num_rectangles (region);
  if (n_rects == 0 || n_spans == 0)
    return;

  /* The main method here is to step along the
   * sorted rectangles and spans in lock step, and
   * clipping the spans that are in the current
   * rectangle before going on to the next rectangle.
   */

  span = spans;
  end_span = spans + n_spans;
  for (i = 0; i < n_rects; i++)
    {
      cairo_region_get_rectangle (region, i, (cairo_rectangle_int_t *) &pbox);

      while ((pbox.y + pbox.height < span->y) || (span->y < pbox.y))
	{
	  /* Skip any rectangles that are above the current span */
	  if (pbox.y + pbox.height < span->y)
	    {
	      i++;
	      if (i == n_rects)
		return;
              cairo_region_get_rectangle (region, i, (cairo_rectangle_int_t *) &pbox);
	    }
	  /* Skip any spans that are above the current rectangle */
	  if (span->y < pbox.y)
	    {
	      span++;
	      if (span == end_span)
		return;
	    }
	}
      
      /* Ok, we got at least one span that might intersect this rectangle. */
      tmpspan = span;
      while ((tmpspan < end_span) &&
	     (tmpspan->y < pbox.y + pbox.height))
	{
	  y = tmpspan->y;
	  left = tmpspan->x;
	  right = left + tmpspan->width; /* right is not in the span! */
	  
	  if ((right > pbox.x) && (left < pbox.x + pbox.width))
	    {
              GdkSpan out_span;

	      clipped_left = MAX (left, pbox.x);
	      clipped_right = MIN (right, pbox.x + pbox.width);
	      
	      out_span.y = y;
	      out_span.x = clipped_left;
	      out_span.width = clipped_right - clipped_left;
	      (*function) (&out_span, data);
	    }
	  
	  tmpspan++;
	}
    }
}

#define __GDK_REGION_GENERIC_C__
#include "gdkaliasdef.c"
