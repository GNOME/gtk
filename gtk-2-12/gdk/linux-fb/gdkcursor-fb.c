/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gdkfb.h"
#include "gdkprivate-fb.h"
#include "gdkcursor.h"

#include "x-cursors.xbm"

static struct {
  const guchar *bits;
  int width, height, hotx, hoty;
  GdkCursor *cursor;
} stock_cursors[] = {
{X_cursor_bits, X_cursor_width, X_cursor_height, X_cursor_x_hot, X_cursor_y_hot},
{X_cursor_mask_bits, X_cursor_mask_width, X_cursor_mask_height, X_cursor_mask_x_hot, X_cursor_mask_y_hot},
{arrow_bits, arrow_width, arrow_height, arrow_x_hot, arrow_y_hot},
{arrow_mask_bits, arrow_mask_width, arrow_mask_height, arrow_mask_x_hot, arrow_mask_y_hot},
{based_arrow_down_bits, based_arrow_down_width, based_arrow_down_height, based_arrow_down_x_hot, based_arrow_down_y_hot},
{based_arrow_down_mask_bits, based_arrow_down_mask_width, based_arrow_down_mask_height, based_arrow_down_mask_x_hot, based_arrow_down_mask_y_hot},
{based_arrow_up_bits, based_arrow_up_width, based_arrow_up_height, based_arrow_up_x_hot, based_arrow_up_y_hot},
{based_arrow_up_mask_bits, based_arrow_up_mask_width, based_arrow_up_mask_height, based_arrow_up_mask_x_hot, based_arrow_up_mask_y_hot},
{boat_bits, boat_width, boat_height, boat_x_hot, boat_y_hot},
{boat_mask_bits, boat_mask_width, boat_mask_height, boat_mask_x_hot, boat_mask_y_hot},
{bogosity_bits, bogosity_width, bogosity_height, bogosity_x_hot, bogosity_y_hot},
{bogosity_mask_bits, bogosity_mask_width, bogosity_mask_height, bogosity_mask_x_hot, bogosity_mask_y_hot},
{bottom_left_corner_bits, bottom_left_corner_width, bottom_left_corner_height, bottom_left_corner_x_hot, bottom_left_corner_y_hot},
{bottom_left_corner_mask_bits, bottom_left_corner_mask_width, bottom_left_corner_mask_height, bottom_left_corner_mask_x_hot, bottom_left_corner_mask_y_hot},
{bottom_right_corner_bits, bottom_right_corner_width, bottom_right_corner_height, bottom_right_corner_x_hot, bottom_right_corner_y_hot},
{bottom_right_corner_mask_bits, bottom_right_corner_mask_width, bottom_right_corner_mask_height, bottom_right_corner_mask_x_hot, bottom_right_corner_mask_y_hot},
{bottom_side_bits, bottom_side_width, bottom_side_height, bottom_side_x_hot, bottom_side_y_hot},
{bottom_side_mask_bits, bottom_side_mask_width, bottom_side_mask_height, bottom_side_mask_x_hot, bottom_side_mask_y_hot},
{bottom_tee_bits, bottom_tee_width, bottom_tee_height, bottom_tee_x_hot, bottom_tee_y_hot},
{bottom_tee_mask_bits, bottom_tee_mask_width, bottom_tee_mask_height, bottom_tee_mask_x_hot, bottom_tee_mask_y_hot},
{box_spiral_bits, box_spiral_width, box_spiral_height, box_spiral_x_hot, box_spiral_y_hot},
{box_spiral_mask_bits, box_spiral_mask_width, box_spiral_mask_height, box_spiral_mask_x_hot, box_spiral_mask_y_hot},
{center_ptr_bits, center_ptr_width, center_ptr_height, center_ptr_x_hot, center_ptr_y_hot},
{center_ptr_mask_bits, center_ptr_mask_width, center_ptr_mask_height, center_ptr_mask_x_hot, center_ptr_mask_y_hot},
{circle_bits, circle_width, circle_height, circle_x_hot, circle_y_hot},
{circle_mask_bits, circle_mask_width, circle_mask_height, circle_mask_x_hot, circle_mask_y_hot},
{clock_bits, clock_width, clock_height, clock_x_hot, clock_y_hot},
{clock_mask_bits, clock_mask_width, clock_mask_height, clock_mask_x_hot, clock_mask_y_hot},
{coffee_mug_bits, coffee_mug_width, coffee_mug_height, coffee_mug_x_hot, coffee_mug_y_hot},
{coffee_mug_mask_bits, coffee_mug_mask_width, coffee_mug_mask_height, coffee_mug_mask_x_hot, coffee_mug_mask_y_hot},
{cross_bits, cross_width, cross_height, cross_x_hot, cross_y_hot},
{cross_mask_bits, cross_mask_width, cross_mask_height, cross_mask_x_hot, cross_mask_y_hot},
{cross_reverse_bits, cross_reverse_width, cross_reverse_height, cross_reverse_x_hot, cross_reverse_y_hot},
{cross_reverse_mask_bits, cross_reverse_mask_width, cross_reverse_mask_height, cross_reverse_mask_x_hot, cross_reverse_mask_y_hot},
{crosshair_bits, crosshair_width, crosshair_height, crosshair_x_hot, crosshair_y_hot},
{crosshair_mask_bits, crosshair_mask_width, crosshair_mask_height, crosshair_mask_x_hot, crosshair_mask_y_hot},
{diamond_cross_bits, diamond_cross_width, diamond_cross_height, diamond_cross_x_hot, diamond_cross_y_hot},
{diamond_cross_mask_bits, diamond_cross_mask_width, diamond_cross_mask_height, diamond_cross_mask_x_hot, diamond_cross_mask_y_hot},
{dot_bits, dot_width, dot_height, dot_x_hot, dot_y_hot},
{dot_mask_bits, dot_mask_width, dot_mask_height, dot_mask_x_hot, dot_mask_y_hot},
{dotbox_bits, dotbox_width, dotbox_height, dotbox_x_hot, dotbox_y_hot},
{dotbox_mask_bits, dotbox_mask_width, dotbox_mask_height, dotbox_mask_x_hot, dotbox_mask_y_hot},
{double_arrow_bits, double_arrow_width, double_arrow_height, double_arrow_x_hot, double_arrow_y_hot},
{double_arrow_mask_bits, double_arrow_mask_width, double_arrow_mask_height, double_arrow_mask_x_hot, double_arrow_mask_y_hot},
{draft_large_bits, draft_large_width, draft_large_height, draft_large_x_hot, draft_large_y_hot},
{draft_large_mask_bits, draft_large_mask_width, draft_large_mask_height, draft_large_mask_x_hot, draft_large_mask_y_hot},
{draft_small_bits, draft_small_width, draft_small_height, draft_small_x_hot, draft_small_y_hot},
{draft_small_mask_bits, draft_small_mask_width, draft_small_mask_height, draft_small_mask_x_hot, draft_small_mask_y_hot},
{draped_box_bits, draped_box_width, draped_box_height, draped_box_x_hot, draped_box_y_hot},
{draped_box_mask_bits, draped_box_mask_width, draped_box_mask_height, draped_box_mask_x_hot, draped_box_mask_y_hot},
{exchange_bits, exchange_width, exchange_height, exchange_x_hot, exchange_y_hot},
{exchange_mask_bits, exchange_mask_width, exchange_mask_height, exchange_mask_x_hot, exchange_mask_y_hot},
{fleur_bits, fleur_width, fleur_height, fleur_x_hot, fleur_y_hot},
{fleur_mask_bits, fleur_mask_width, fleur_mask_height, fleur_mask_x_hot, fleur_mask_y_hot},
{gobbler_bits, gobbler_width, gobbler_height, gobbler_x_hot, gobbler_y_hot},
{gobbler_mask_bits, gobbler_mask_width, gobbler_mask_height, gobbler_mask_x_hot, gobbler_mask_y_hot},
{gumby_bits, gumby_width, gumby_height, gumby_x_hot, gumby_y_hot},
{gumby_mask_bits, gumby_mask_width, gumby_mask_height, gumby_mask_x_hot, gumby_mask_y_hot},
{hand1_bits, hand1_width, hand1_height, hand1_x_hot, hand1_y_hot},
{hand1_mask_bits, hand1_mask_width, hand1_mask_height, hand1_mask_x_hot, hand1_mask_y_hot},
{hand2_bits, hand2_width, hand2_height, hand2_x_hot, hand2_y_hot},
{hand2_mask_bits, hand2_mask_width, hand2_mask_height, hand2_mask_x_hot, hand2_mask_y_hot},
{heart_bits, heart_width, heart_height, heart_x_hot, heart_y_hot},
{heart_mask_bits, heart_mask_width, heart_mask_height, heart_mask_x_hot, heart_mask_y_hot},
{icon_bits, icon_width, icon_height, icon_x_hot, icon_y_hot},
{icon_mask_bits, icon_mask_width, icon_mask_height, icon_mask_x_hot, icon_mask_y_hot},
{iron_cross_bits, iron_cross_width, iron_cross_height, iron_cross_x_hot, iron_cross_y_hot},
{iron_cross_mask_bits, iron_cross_mask_width, iron_cross_mask_height, iron_cross_mask_x_hot, iron_cross_mask_y_hot},
{left_ptr_bits, left_ptr_width, left_ptr_height, left_ptr_x_hot, left_ptr_y_hot},
{left_ptr_mask_bits, left_ptr_mask_width, left_ptr_mask_height, left_ptr_mask_x_hot, left_ptr_mask_y_hot},
{left_side_bits, left_side_width, left_side_height, left_side_x_hot, left_side_y_hot},
{left_side_mask_bits, left_side_mask_width, left_side_mask_height, left_side_mask_x_hot, left_side_mask_y_hot},
{left_tee_bits, left_tee_width, left_tee_height, left_tee_x_hot, left_tee_y_hot},
{left_tee_mask_bits, left_tee_mask_width, left_tee_mask_height, left_tee_mask_x_hot, left_tee_mask_y_hot},
{leftbutton_bits, leftbutton_width, leftbutton_height, leftbutton_x_hot, leftbutton_y_hot},
{leftbutton_mask_bits, leftbutton_mask_width, leftbutton_mask_height, leftbutton_mask_x_hot, leftbutton_mask_y_hot},
{ll_angle_bits, ll_angle_width, ll_angle_height, ll_angle_x_hot, ll_angle_y_hot},
{ll_angle_mask_bits, ll_angle_mask_width, ll_angle_mask_height, ll_angle_mask_x_hot, ll_angle_mask_y_hot},
{lr_angle_bits, lr_angle_width, lr_angle_height, lr_angle_x_hot, lr_angle_y_hot},
{lr_angle_mask_bits, lr_angle_mask_width, lr_angle_mask_height, lr_angle_mask_x_hot, lr_angle_mask_y_hot},
{man_bits, man_width, man_height, man_x_hot, man_y_hot},
{man_mask_bits, man_mask_width, man_mask_height, man_mask_x_hot, man_mask_y_hot},
{middlebutton_bits, middlebutton_width, middlebutton_height, middlebutton_x_hot, middlebutton_y_hot},
{middlebutton_mask_bits, middlebutton_mask_width, middlebutton_mask_height, middlebutton_mask_x_hot, middlebutton_mask_y_hot},
{mouse_bits, mouse_width, mouse_height, mouse_x_hot, mouse_y_hot},
{mouse_mask_bits, mouse_mask_width, mouse_mask_height, mouse_mask_x_hot, mouse_mask_y_hot},
{pencil_bits, pencil_width, pencil_height, pencil_x_hot, pencil_y_hot},
{pencil_mask_bits, pencil_mask_width, pencil_mask_height, pencil_mask_x_hot, pencil_mask_y_hot},
{pirate_bits, pirate_width, pirate_height, pirate_x_hot, pirate_y_hot},
{pirate_mask_bits, pirate_mask_width, pirate_mask_height, pirate_mask_x_hot, pirate_mask_y_hot},
{plus_bits, plus_width, plus_height, plus_x_hot, plus_y_hot},
{plus_mask_bits, plus_mask_width, plus_mask_height, plus_mask_x_hot, plus_mask_y_hot},
{question_arrow_bits, question_arrow_width, question_arrow_height, question_arrow_x_hot, question_arrow_y_hot},
{question_arrow_mask_bits, question_arrow_mask_width, question_arrow_mask_height, question_arrow_mask_x_hot, question_arrow_mask_y_hot},
{right_ptr_bits, right_ptr_width, right_ptr_height, right_ptr_x_hot, right_ptr_y_hot},
{right_ptr_mask_bits, right_ptr_mask_width, right_ptr_mask_height, right_ptr_mask_x_hot, right_ptr_mask_y_hot},
{right_side_bits, right_side_width, right_side_height, right_side_x_hot, right_side_y_hot},
{right_side_mask_bits, right_side_mask_width, right_side_mask_height, right_side_mask_x_hot, right_side_mask_y_hot},
{right_tee_bits, right_tee_width, right_tee_height, right_tee_x_hot, right_tee_y_hot},
{right_tee_mask_bits, right_tee_mask_width, right_tee_mask_height, right_tee_mask_x_hot, right_tee_mask_y_hot},
{rightbutton_bits, rightbutton_width, rightbutton_height, rightbutton_x_hot, rightbutton_y_hot},
{rightbutton_mask_bits, rightbutton_mask_width, rightbutton_mask_height, rightbutton_mask_x_hot, rightbutton_mask_y_hot},
{rtl_logo_bits, rtl_logo_width, rtl_logo_height, rtl_logo_x_hot, rtl_logo_y_hot},
{rtl_logo_mask_bits, rtl_logo_mask_width, rtl_logo_mask_height, rtl_logo_mask_x_hot, rtl_logo_mask_y_hot},
{sailboat_bits, sailboat_width, sailboat_height, sailboat_x_hot, sailboat_y_hot},
{sailboat_mask_bits, sailboat_mask_width, sailboat_mask_height, sailboat_mask_x_hot, sailboat_mask_y_hot},
{sb_down_arrow_bits, sb_down_arrow_width, sb_down_arrow_height, sb_down_arrow_x_hot, sb_down_arrow_y_hot},
{sb_down_arrow_mask_bits, sb_down_arrow_mask_width, sb_down_arrow_mask_height, sb_down_arrow_mask_x_hot, sb_down_arrow_mask_y_hot},
{sb_h_double_arrow_bits, sb_h_double_arrow_width, sb_h_double_arrow_height, sb_h_double_arrow_x_hot, sb_h_double_arrow_y_hot},
{sb_h_double_arrow_mask_bits, sb_h_double_arrow_mask_width, sb_h_double_arrow_mask_height, sb_h_double_arrow_mask_x_hot, sb_h_double_arrow_mask_y_hot},
{sb_left_arrow_bits, sb_left_arrow_width, sb_left_arrow_height, sb_left_arrow_x_hot, sb_left_arrow_y_hot},
{sb_left_arrow_mask_bits, sb_left_arrow_mask_width, sb_left_arrow_mask_height, sb_left_arrow_mask_x_hot, sb_left_arrow_mask_y_hot},
{sb_right_arrow_bits, sb_right_arrow_width, sb_right_arrow_height, sb_right_arrow_x_hot, sb_right_arrow_y_hot},
{sb_right_arrow_mask_bits, sb_right_arrow_mask_width, sb_right_arrow_mask_height, sb_right_arrow_mask_x_hot, sb_right_arrow_mask_y_hot},
{sb_up_arrow_bits, sb_up_arrow_width, sb_up_arrow_height, sb_up_arrow_x_hot, sb_up_arrow_y_hot},
{sb_up_arrow_mask_bits, sb_up_arrow_mask_width, sb_up_arrow_mask_height, sb_up_arrow_mask_x_hot, sb_up_arrow_mask_y_hot},
{sb_v_double_arrow_bits, sb_v_double_arrow_width, sb_v_double_arrow_height, sb_v_double_arrow_x_hot, sb_v_double_arrow_y_hot},
{sb_v_double_arrow_mask_bits, sb_v_double_arrow_mask_width, sb_v_double_arrow_mask_height, sb_v_double_arrow_mask_x_hot, sb_v_double_arrow_mask_y_hot},
{shuttle_bits, shuttle_width, shuttle_height, shuttle_x_hot, shuttle_y_hot},
{shuttle_mask_bits, shuttle_mask_width, shuttle_mask_height, shuttle_mask_x_hot, shuttle_mask_y_hot},
{sizing_bits, sizing_width, sizing_height, sizing_x_hot, sizing_y_hot},
{sizing_mask_bits, sizing_mask_width, sizing_mask_height, sizing_mask_x_hot, sizing_mask_y_hot},
{spider_bits, spider_width, spider_height, spider_x_hot, spider_y_hot},
{spider_mask_bits, spider_mask_width, spider_mask_height, spider_mask_x_hot, spider_mask_y_hot},
{spraycan_bits, spraycan_width, spraycan_height, spraycan_x_hot, spraycan_y_hot},
{spraycan_mask_bits, spraycan_mask_width, spraycan_mask_height, spraycan_mask_x_hot, spraycan_mask_y_hot},
{star_bits, star_width, star_height, star_x_hot, star_y_hot},
{star_mask_bits, star_mask_width, star_mask_height, star_mask_x_hot, star_mask_y_hot},
{target_bits, target_width, target_height, target_x_hot, target_y_hot},
{target_mask_bits, target_mask_width, target_mask_height, target_mask_x_hot, target_mask_y_hot},
{tcross_bits, tcross_width, tcross_height, tcross_x_hot, tcross_y_hot},
{tcross_mask_bits, tcross_mask_width, tcross_mask_height, tcross_mask_x_hot, tcross_mask_y_hot},
{top_left_arrow_bits, top_left_arrow_width, top_left_arrow_height, top_left_arrow_x_hot, top_left_arrow_y_hot},
{top_left_arrow_mask_bits, top_left_arrow_mask_width, top_left_arrow_mask_height, top_left_arrow_mask_x_hot, top_left_arrow_mask_y_hot},
{top_left_corner_bits, top_left_corner_width, top_left_corner_height, top_left_corner_x_hot, top_left_corner_y_hot},
{top_left_corner_mask_bits, top_left_corner_mask_width, top_left_corner_mask_height, top_left_corner_mask_x_hot, top_left_corner_mask_y_hot},
{top_right_corner_bits, top_right_corner_width, top_right_corner_height, top_right_corner_x_hot, top_right_corner_y_hot},
{top_right_corner_mask_bits, top_right_corner_mask_width, top_right_corner_mask_height, top_right_corner_mask_x_hot, top_right_corner_mask_y_hot},
{top_side_bits, top_side_width, top_side_height, top_side_x_hot, top_side_y_hot},
{top_side_mask_bits, top_side_mask_width, top_side_mask_height, top_side_mask_x_hot, top_side_mask_y_hot},
{top_tee_bits, top_tee_width, top_tee_height, top_tee_x_hot, top_tee_y_hot},
{top_tee_mask_bits, top_tee_mask_width, top_tee_mask_height, top_tee_mask_x_hot, top_tee_mask_y_hot},
{trek_bits, trek_width, trek_height, trek_x_hot, trek_y_hot},
{trek_mask_bits, trek_mask_width, trek_mask_height, trek_mask_x_hot, trek_mask_y_hot},
{ul_angle_bits, ul_angle_width, ul_angle_height, ul_angle_x_hot, ul_angle_y_hot},
{ul_angle_mask_bits, ul_angle_mask_width, ul_angle_mask_height, ul_angle_mask_x_hot, ul_angle_mask_y_hot},
{umbrella_bits, umbrella_width, umbrella_height, umbrella_x_hot, umbrella_y_hot},
{umbrella_mask_bits, umbrella_mask_width, umbrella_mask_height, umbrella_mask_x_hot, umbrella_mask_y_hot},
{ur_angle_bits, ur_angle_width, ur_angle_height, ur_angle_x_hot, ur_angle_y_hot},
{ur_angle_mask_bits, ur_angle_mask_width, ur_angle_mask_height, ur_angle_mask_x_hot, ur_angle_mask_y_hot},
{watch_bits, watch_width, watch_height, watch_x_hot, watch_y_hot},
{watch_mask_bits, watch_mask_width, watch_mask_height, watch_mask_x_hot, watch_mask_y_hot},
{xterm_bits, xterm_width, xterm_height, xterm_x_hot, xterm_y_hot},
{xterm_mask_bits, xterm_mask_width, xterm_mask_height, xterm_mask_x_hot, xterm_mask_y_hot}
};

GdkCursor*
gdk_cursor_new_for_display (GdkDisplay    *display,
			    GdkCursorType  cursor_type)
{
  GdkCursor *cursor;
  
  if (cursor_type >= sizeof(stock_cursors)/sizeof(stock_cursors[0]))
    return NULL;

  cursor = stock_cursors[cursor_type].cursor;
  if (!cursor)
    {
      GdkPixmap *tmp_pm, *pm, *mask;
      GdkGC *copy_gc;
      char *data;
     
      tmp_pm = gdk_bitmap_create_from_data (_gdk_parent_root,
					    stock_cursors[cursor_type].bits,
					    stock_cursors[cursor_type].width,
					    stock_cursors[cursor_type].height);

      /* Create an empty bitmap the size of the mask */
      data = g_malloc0 (((stock_cursors[cursor_type+1].width+7)/8) * stock_cursors[cursor_type+1].height);
      pm = gdk_bitmap_create_from_data (_gdk_parent_root,
					data,
					stock_cursors[cursor_type+1].width,
					stock_cursors[cursor_type+1].height);
      copy_gc = gdk_gc_new (pm);
      gdk_draw_drawable(pm,
			copy_gc,
			tmp_pm,
			0, 0,
			stock_cursors[cursor_type+1].hotx - stock_cursors[cursor_type].hotx,
			stock_cursors[cursor_type+1].hoty - stock_cursors[cursor_type].hoty,
			stock_cursors[cursor_type].width,
			stock_cursors[cursor_type].height);
      gdk_pixmap_unref (tmp_pm);
      g_free (data);
      gdk_gc_unref (copy_gc);

      mask =  gdk_bitmap_create_from_data (_gdk_parent_root,
					   stock_cursors[cursor_type+1].bits,
					   stock_cursors[cursor_type+1].width,
					   stock_cursors[cursor_type+1].height);

      cursor = gdk_cursor_new_from_pixmap (pm, mask, NULL, NULL,
				     stock_cursors[cursor_type+1].hotx,
				     stock_cursors[cursor_type+1].hoty);

      stock_cursors[cursor_type].cursor = cursor;
    }
  return gdk_cursor_ref (cursor);
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap      *source,
			    GdkPixmap      *mask,
			    const GdkColor *fg,
			    const GdkColor *bg,
			    gint            x,
			    gint            y)
{
  GdkCursorPrivateFB *private;
  GdkCursor *cursor;

  g_return_val_if_fail (source != NULL, NULL);

  private = g_new (GdkCursorPrivateFB, 1);
  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  private->cursor = gdk_pixmap_ref (source);
  private->mask = gdk_pixmap_ref (mask);
  private->hot_x = x;
  private->hot_y = y;
  
  return cursor;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivateFB *private;

  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count == 0);

  private = (GdkCursorPrivateFB *) cursor;

  if (private->mask)
    gdk_pixmap_unref (private->mask);
  gdk_pixmap_unref (private->cursor);
  
  g_free (private);
}

/* Global data to keep track of cursor */
static GdkPixmap *last_contents = NULL;
static GdkPoint last_location, last_contents_size;
static GdkCursor *last_cursor = NULL;
static GdkFBDrawingContext *gdk_fb_cursor_dc = NULL;
static GdkFBDrawingContext cursor_dc_dat;
static GdkGC *cursor_gc;
static gint cursor_visibility_count = 1;

static GdkFBDrawingContext *
gdk_fb_cursor_dc_reset (void)
{
  if (gdk_fb_cursor_dc)
    gdk_fb_drawing_context_finalize (gdk_fb_cursor_dc);

  gdk_fb_cursor_dc = &cursor_dc_dat;
  gdk_fb_drawing_context_init (gdk_fb_cursor_dc,
			       GDK_DRAWABLE_IMPL(_gdk_parent_root),
			       cursor_gc,
			       TRUE,
			       FALSE);

  return gdk_fb_cursor_dc;
}

void
gdk_fb_cursor_hide (void)
{
  GdkFBDrawingContext *mydc = gdk_fb_cursor_dc;

  cursor_visibility_count--;
  g_assert (cursor_visibility_count <= 0);
  
  if (cursor_visibility_count < 0)
    return;

  if (!mydc)
    mydc = gdk_fb_cursor_dc_reset ();

  if (last_contents)
    {
      gdk_gc_set_clip_mask (cursor_gc, NULL);
      /* Restore old picture */
      gdk_fb_draw_drawable_3 (GDK_DRAWABLE_IMPL(_gdk_parent_root),
			      cursor_gc,
			      GDK_DRAWABLE_IMPL(last_contents),
			      mydc,
			      0, 0,
			      last_location.x,
			      last_location.y,
			      last_contents_size.x,
			      last_contents_size.y);
      gdk_shadow_fb_update (last_location.x, last_location.y,
			    last_location.x + last_contents_size.x,
			    last_location.y + last_contents_size.y);
    }
}

void
gdk_fb_cursor_invalidate (void)
{
  if (last_contents)
    {
      gdk_pixmap_unref (last_contents);
      last_contents = NULL;
    }
}

void
gdk_fb_cursor_unhide (void)
{
  GdkFBDrawingContext *mydc = gdk_fb_cursor_dc;
  GdkCursorPrivateFB *last_private;
  GdkDrawableFBData *pixmap_last;
  
  last_private = GDK_CURSOR_FB (last_cursor);
  cursor_visibility_count++;
  g_assert (cursor_visibility_count <= 1);
  if (cursor_visibility_count < 1)
    return;

  if (!mydc)
    mydc = gdk_fb_cursor_dc_reset ();

  if (last_cursor)
    {
      pixmap_last = GDK_DRAWABLE_IMPL_FBDATA (last_private->cursor);
      
      if (!last_contents ||
	  pixmap_last->width > GDK_DRAWABLE_IMPL_FBDATA (last_contents)->width ||
	  pixmap_last->height > GDK_DRAWABLE_IMPL_FBDATA (last_contents)->height)
	{
	  if (last_contents)
	    gdk_pixmap_unref (last_contents);

	  last_contents = gdk_pixmap_new (_gdk_parent_root,
					  pixmap_last->width,
					  pixmap_last->height,
					  GDK_DRAWABLE_IMPL_FBDATA (_gdk_parent_root)->depth);
	}

      gdk_gc_set_clip_mask (cursor_gc, NULL);
      gdk_fb_draw_drawable_2 (GDK_DRAWABLE_IMPL (last_contents),
			      cursor_gc,
			      GDK_DRAWABLE_IMPL (_gdk_parent_root),
			      last_location.x,
			      last_location.y,
			      0, 0,
			      pixmap_last->width,
			      pixmap_last->height,
			      TRUE, FALSE);
      last_contents_size.x = pixmap_last->width;
      last_contents_size.y = pixmap_last->height;
      
      gdk_gc_set_clip_mask (cursor_gc, last_private->mask);
      gdk_gc_set_clip_origin (cursor_gc,
			      last_location.x,
			      last_location.y);

      gdk_fb_cursor_dc_reset ();
      gdk_fb_draw_drawable_3 (GDK_DRAWABLE_IMPL (_gdk_parent_root),
			      cursor_gc,
			      GDK_DRAWABLE_IMPL (last_private->cursor),
			      mydc,
			      0, 0,
			      last_location.x, last_location.y,
			      pixmap_last->width,
			      pixmap_last->height);
      gdk_shadow_fb_update (last_location.x, last_location.y,
			    last_location.x + pixmap_last->width,
			    last_location.y + pixmap_last->height);
    }
  else
    gdk_fb_cursor_invalidate ();
}

gboolean
gdk_fb_cursor_region_need_hide (GdkRegion *region)
{
  GdkRectangle testme;

  if (!last_cursor)
    return FALSE;

  testme.x = last_location.x;
  testme.y = last_location.y;
  testme.width = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->width;
  testme.height = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->height;

  return (gdk_region_rect_in (region, &testme) != GDK_OVERLAP_RECTANGLE_OUT);
}

gboolean
gdk_fb_cursor_need_hide (GdkRectangle *rect)
{
  GdkRectangle testme;

  if (!last_cursor)
    return FALSE;

  testme.x = last_location.x;
  testme.y = last_location.y;
  testme.width = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->width;
  testme.height = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->height;

  return gdk_rectangle_intersect (rect, &testme, &testme);
}

void
gdk_fb_get_cursor_rect (GdkRectangle *rect)
{
  if (last_cursor)
    {
      rect->x = last_location.x;
      rect->y = last_location.y;
      rect->width = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->width;
      rect->height = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->height;
    }
  else
    {
      rect->x = rect->y = -1;
      rect->width = rect->height = 0;
    }
}

void
gdk_fb_cursor_move (gint x, gint y, GdkWindow *in_window)
{
  GdkCursor *the_cursor;

  if (!cursor_gc)
    {
      GdkColor white, black;
      cursor_gc = gdk_gc_new (_gdk_parent_root);
      gdk_color_black (gdk_colormap_get_system (), &black);
      gdk_color_white (gdk_colormap_get_system (), &white);
      gdk_gc_set_foreground (cursor_gc, &black);
      gdk_gc_set_background (cursor_gc, &white);
    }

  gdk_fb_cursor_hide ();

  if (_gdk_fb_pointer_grab_window)
    {
      if (_gdk_fb_pointer_grab_cursor)
	the_cursor = _gdk_fb_pointer_grab_cursor;
      else
	{
	  GdkWindow *win = _gdk_fb_pointer_grab_window;
	  while (!GDK_WINDOW_IMPL_FBDATA (win)->cursor && GDK_WINDOW_OBJECT (win)->parent)
	    win = (GdkWindow *)GDK_WINDOW_OBJECT (win)->parent;
	  the_cursor = GDK_WINDOW_IMPL_FBDATA (win)->cursor;
	}
    }
  else
    {
      while (!GDK_WINDOW_IMPL_FBDATA (in_window)->cursor && GDK_WINDOW_P (in_window)->parent)
	in_window = (GdkWindow *)GDK_WINDOW_P (in_window)->parent;
      the_cursor = GDK_WINDOW_IMPL_FBDATA (in_window)->cursor;
    }

  last_location.x = x - GDK_CURSOR_FB (the_cursor)->hot_x;
  last_location.y = y - GDK_CURSOR_FB (the_cursor)->hot_y;

  if (the_cursor)
    gdk_cursor_ref (the_cursor);
  if (last_cursor)
    gdk_cursor_unref (last_cursor);
  last_cursor = the_cursor;

  gdk_fb_cursor_unhide ();
}

void
gdk_fb_cursor_reset(void)
{
  GdkWindow *win = gdk_window_at_pointer (NULL, NULL);
  gint x, y;

  gdk_fb_mouse_get_info (&x, &y, NULL);
  gdk_fb_cursor_move (x, y, win);
}

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  return gdk_display_get_default ();
}
