#include <config.h>
#include "gdkprivate-fb.h"

void
gdk_window_scroll (GdkWindow *window,
                   gint       dx,
                   gint       dy)
{
  GdkWindowObject *private = GDK_WINDOW_P (window);
  GdkRegion *invalidate_region;
  GdkRectangle dest_rect;
  GdkRectangle clip_rect;
  GList *tmp_list;
  gboolean handle_cursor;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  clip_rect.x = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_x;
  clip_rect.y = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_y;
  clip_rect.width = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_x - GDK_DRAWABLE_IMPL_FBDATA (window)->llim_x;
  clip_rect.height = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_y - GDK_DRAWABLE_IMPL_FBDATA (window)->llim_y;
  handle_cursor = gdk_fb_cursor_need_hide (&clip_rect);
  clip_rect.x -= GDK_DRAWABLE_IMPL_FBDATA (window)->abs_x;
  clip_rect.y -= GDK_DRAWABLE_IMPL_FBDATA (window)->abs_y;
  invalidate_region = gdk_region_rectangle (&clip_rect);
      
  dest_rect = clip_rect;
  dest_rect.x += dx;
  dest_rect.y += dy;
  gdk_rectangle_intersect (&dest_rect, &clip_rect, &dest_rect);

  if (handle_cursor)
    gdk_fb_cursor_hide ();

  if (dest_rect.width > 0 && dest_rect.height > 0)
    {
      GdkRegion *tmp_region;

      tmp_region = gdk_region_rectangle (&dest_rect);
      gdk_region_subtract (invalidate_region, tmp_region);
      gdk_region_destroy (tmp_region);

      gdk_fb_draw_drawable_2 (GDK_DRAWABLE_IMPL(window),
			      _gdk_fb_screen_gc,
			      GDK_DRAWABLE_IMPL(window),
			      dest_rect.x - dx,
			      dest_rect.y - dy,
			      dest_rect.x, dest_rect.y,
			      dest_rect.width, dest_rect.height,
			      FALSE, FALSE);
      gdk_shadow_fb_update (dest_rect.x - dx, dest_rect.y - dy,
			    dest_rect.x - dx + dest_rect.width,
			    dest_rect.y - dy + dest_rect.height);
    }
  
  gdk_window_invalidate_region (window, invalidate_region, TRUE);
  gdk_region_destroy (invalidate_region);

  for (tmp_list = private->children; tmp_list; tmp_list = tmp_list->next)
    gdk_fb_window_move_resize (tmp_list->data,
			       GDK_WINDOW_OBJECT(tmp_list->data)->x + dx,
			       GDK_WINDOW_OBJECT(tmp_list->data)->y + dy,
			       GDK_DRAWABLE_IMPL_FBDATA(tmp_list->data)->width,
			       GDK_DRAWABLE_IMPL_FBDATA(tmp_list->data)->height,
			       FALSE);

  if (handle_cursor)
    gdk_fb_cursor_unhide ();
}
