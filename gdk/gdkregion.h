#ifndef __GDK_REGION_H__
#define __GDK_REGION_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* GC fill rule for polygons
 *  EvenOddRule
 *  WindingRule
 */
typedef enum
{
  GDK_EVEN_ODD_RULE,
  GDK_WINDING_RULE
} GdkFillRule;

/* Types of overlapping between a rectangle and a region
 * GDK_OVERLAP_RECTANGLE_IN: rectangle is in region
 * GDK_OVERLAP_RECTANGLE_OUT: rectangle in not in region
 * GDK_OVERLAP_RECTANGLE_PART: rectangle in partially in region
 */
typedef enum
{
  GDK_OVERLAP_RECTANGLE_IN,
  GDK_OVERLAP_RECTANGLE_OUT,
  GDK_OVERLAP_RECTANGLE_PART
} GdkOverlapType;

typedef void (*GdkSpanFunc) (GdkSpan *span,
			     gpointer data);

GdkRegion *gdk_region_new       (void);
GdkRegion *gdk_region_polygon   (GdkPoint     *points,
				 gint          npoints,
				 GdkFillRule   fill_rule);
GdkRegion *gdk_region_copy      (GdkRegion    *region);
GdkRegion *gdk_region_rectangle (GdkRectangle *rectangle);
void       gdk_region_destroy   (GdkRegion    *region);

void	       gdk_region_get_clipbox    (GdkRegion     *region,
                                          GdkRectangle  *rectangle);
void           gdk_region_get_rectangles (GdkRegion     *region,
                                          GdkRectangle **rectangles,
                                          gint          *n_rectangles);

gboolean       gdk_region_empty    (GdkRegion    *region);
gboolean       gdk_region_equal    (GdkRegion    *region1,
				    GdkRegion    *region2);
gboolean       gdk_region_point_in (GdkRegion    *region,
				    int           x,
				    int           y);
GdkOverlapType gdk_region_rect_in  (GdkRegion    *region,
				    GdkRectangle *rect);

void gdk_region_offset          (GdkRegion    *region,
				 gint          dx,
				 gint          dy);
void gdk_region_shrink          (GdkRegion    *region,
				 gint          dx,
				 gint          dy);
void gdk_region_union_with_rect (GdkRegion    *region,
				 GdkRectangle *rect);
void gdk_region_intersect       (GdkRegion    *source1,
				 GdkRegion    *source2);
void gdk_region_union           (GdkRegion    *source1,
			 	 GdkRegion    *source2);
void gdk_region_subtract        (GdkRegion    *source1,
				 GdkRegion    *source2);
void gdk_region_xor             (GdkRegion    *source1,
				 GdkRegion    *source2);

void gdk_region_spans_intersect_foreach (GdkRegion   *region,
					 GdkSpan     *spans,
					 int          n_spans,
					 gboolean     sorted,
					 GdkSpanFunc  function,
					 gpointer     data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_REGION_H__ */

