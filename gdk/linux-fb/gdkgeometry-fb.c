#include "gdkprivate-fb.h"

void
gdk_window_scroll (GdkWindow *window,
                   gint       dx,
                   gint       dy)
{
  GdkWindowPrivate *private = GDK_WINDOW_P(window);
  GdkRegion *invalidate_region;
  GdkRectangle dest_rect;
  GdkRectangle clip_rect;
  GList *tmp_list;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_DRAWABLE_DESTROYED (window))
    return;

  clip_rect.x = GDK_DRAWABLE_FBDATA(window)->llim_x - GDK_DRAWABLE_FBDATA(window)->abs_x;
  clip_rect.y = GDK_DRAWABLE_FBDATA(window)->llim_y - GDK_DRAWABLE_FBDATA(window)->abs_y;
  clip_rect.width = GDK_DRAWABLE_FBDATA(window)->lim_x - GDK_DRAWABLE_FBDATA(window)->llim_x;
  clip_rect.height = GDK_DRAWABLE_FBDATA(window)->lim_y - GDK_DRAWABLE_FBDATA(window)->llim_y;
  invalidate_region = gdk_region_rectangle (&clip_rect);
      
  dest_rect = clip_rect;
  dest_rect.x += dx;
  dest_rect.y += dy;
  gdk_rectangle_intersect (&dest_rect, &clip_rect, &dest_rect);

  if (dest_rect.width > 0 && dest_rect.height > 0)
    {
      GdkRegion *tmp_region;

      tmp_region = gdk_region_rectangle (&dest_rect);
      gdk_region_subtract (invalidate_region, tmp_region);
      gdk_region_destroy (tmp_region);
	  
      gdk_fb_draw_drawable_2(window, NULL, window, dest_rect.x - dx, dest_rect.y - dy, dest_rect.x, dest_rect.y,
			     dest_rect.width, dest_rect.height,
			     FALSE, FALSE);
    }

  gdk_window_invalidate_region (window, invalidate_region, TRUE);
  gdk_region_destroy (invalidate_region);

  for(tmp_list = private->children; tmp_list; tmp_list = tmp_list->next)
    gdk_window_move (tmp_list->data, GDK_WINDOW_P(tmp_list->data)->x + dx, GDK_WINDOW_P(tmp_list->data)->y + dy);
}
