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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdkfb.h"
#include "gdkprivate-fb.h"
#include "gdkcursor.h"

#include "/home/sopwith/bin/t.xbm"

static struct {
  const guchar *bits;
  int width, height, hotx, hoty;
  GdkPixmap *pm;
} stock_cursors[] = {
  {X_cursor_bits, 14, 14, 6, 8},
  {X_cursor_mask_bits, 16, 16, 7, 9},
  {arrow_bits, 14, 14, 13, 14},
  {arrow_mask_bits, 16, 16, 14, 15},
  {based_arrow_down_bits, 8, 10, 3, 1},
  {based_arrow_down_mask_bits, 10, 12, 4, 2},
  {based_arrow_up_bits, 8, 10, 3, 1},
  {based_arrow_up_mask_bits, 10, 12, 4, 2},
  {boat_bits, 16, 8, 14, 5},
  {boat_mask_bits, 16, 9, 14, 5},
  {bogosity_bits, 13, 14, 6, 8},
  {bogosity_mask_bits, 15, 16, 7, 9},
  {bottom_left_corner_bits, 14, 14, 0, 1},
  {bottom_left_corner_mask_bits, 16, 16, 1, 2},
  {bottom_right_corner_bits, 14, 14, 13, 1},
  {bottom_right_corner_mask_bits, 16, 16, 14, 2},
  {bottom_side_bits, 13, 14, 6, 1},
  {bottom_side_mask_bits, 15, 16, 7, 2},
  {bottom_tee_bits, 14, 10, 7, 1},
  {bottom_tee_mask_bits, 16, 12, 8, 2},
  {box_spiral_bits, 15, 16, 8, 8},
  {box_spiral_mask_bits, 16, 16, 8, 8},
  {center_ptr_bits, 10, 14, 4, 14},
  {center_ptr_mask_bits, 12, 16, 5, 15},
  {circle_bits, 14, 14, 7, 7},
  {circle_mask_bits, 16, 16, 8, 8},
  {clock_bits, 14, 16, 6, 13},
  {clock_mask_bits, 15, 16, 6, 13},
  {coffee_mug_bits, 15, 16, 7, 7},
  {coffee_mug_mask_bits, 16, 16, 7, 7},
  {cross_bits, 16, 15, 7, 8},
  {cross_mask_bits, 16, 16, 7, 9},
  {cross_reverse_bits, 16, 15, 7, 8},
  {cross_reverse_mask_bits, 16, 15, 7, 8},
  {crosshair_bits, 16, 15, 7, 8},
  {crosshair_mask_bits, 16, 16, 7, 9},
  {diamond_cross_bits, 15, 15, 7, 8},
  {diamond_cross_mask_bits, 16, 16, 7, 9},
  {dot_bits, 10, 10, 5, 5},
  {dot_mask_bits, 12, 12, 6, 6},
  {dotbox_bits, 12, 12, 6, 7},
  {dotbox_mask_bits, 14, 14, 7, 8},
  {double_arrow_bits, 10, 14, 5, 7},
  {double_arrow_mask_bits, 12, 16, 6, 8},
  {draft_large_bits, 15, 15, 14, 15},
  {draft_large_mask_bits, 15, 16, 14, 16},
  {draft_small_bits, 15, 15, 14, 15},
  {draft_small_mask_bits, 15, 15, 14, 15},
  {draped_box_bits, 12, 12, 6, 7},
  {draped_box_mask_bits, 14, 14, 7, 8},
  {exchange_bits, 14, 14, 6, 8},
  {exchange_mask_bits, 16, 16, 7, 9},
  {fleur_bits, 14, 14, 7, 7},
  {fleur_mask_bits, 16, 16, 8, 8},
  {gobbler_bits, 16, 15, 14, 13},
  {gobbler_mask_bits, 16, 16, 14, 13},
  {gumby_bits, 16, 16, 2, 16},
  {gumby_mask_bits, 16, 16, 2, 16},
  {hand1_bits, 13, 16, 12, 16},
  {hand1_mask_bits, 13, 16, 12, 16},
  {hand2_bits, 15, 14, 0, 14},
  {hand2_mask_bits, 16, 16, 0, 15},
  {heart_bits, 15, 14, 6, 6},
  {heart_mask_bits, 15, 14, 6, 6},
  {icon_bits, 16, 16, 8, 8},
  {icon_mask_bits, 16, 16, 8, 8},
  {iron_cross_bits, 14, 14, 7, 8},
  {iron_cross_mask_bits, 16, 16, 8, 9},
  {left_ptr_bits, 8, 14, 0, 14},
  {left_ptr_mask_bits, 10, 16, 1, 15},
  {left_side_bits, 14, 13, 0, 7},
  {left_side_mask_bits, 16, 15, 1, 8},
  {left_tee_bits, 10, 14, 0, 7},
  {left_tee_mask_bits, 12, 16, 1, 8},
  {leftbutton_bits, 16, 16, 8, 8},
  {leftbutton_mask_bits, 15, 16, 8, 8},
  {ll_angle_bits, 10, 10, 0, 1},
  {ll_angle_mask_bits, 12, 12, 1, 2},
  {lr_angle_bits, 10, 10, 9, 1},
  {lr_angle_mask_bits, 12, 12, 10, 2},
  {man_bits, 16, 16, 14, 11},
  {man_mask_bits, 16, 16, 14, 11},
  {middlebutton_bits, 16, 16, 8, 8},
  {middlebutton_mask_bits, 15, 16, 8, 8},
  {mouse_bits, 15, 14, 4, 13},
  {mouse_mask_bits, 16, 16, 4, 15},
  {pencil_bits, 11, 16, 10, 1},
  {pencil_mask_bits, 13, 16, 11, 1},
  {pirate_bits, 15, 16, 7, 4},
  {pirate_mask_bits, 16, 16, 7, 4},
  {plus_bits, 10, 10, 4, 5},
  {plus_mask_bits, 12, 12, 5, 6},
  {question_arrow_bits, 9, 15, 4, 8},
  {question_arrow_mask_bits, 11, 16, 5, 8},
  {right_ptr_bits, 8, 14, 7, 14},
  {right_ptr_mask_bits, 10, 16, 8, 15},
  {right_side_bits, 14, 13, 13, 7},
  {right_side_mask_bits, 16, 15, 14, 8},
  {right_tee_bits, 10, 14, 9, 7},
  {right_tee_mask_bits, 12, 16, 10, 8},
  {rightbutton_bits, 16, 16, 8, 8},
  {rightbutton_mask_bits, 15, 16, 8, 8},
  {rtl_logo_bits, 14, 14, 6, 8},
  {rtl_logo_mask_bits, 16, 16, 7, 9},
  {sailboat_bits, 12, 13, 6, 14},
  {sailboat_mask_bits, 16, 16, 8, 16},
  {sb_down_arrow_bits, 7, 15, 3, 0},
  {sb_down_arrow_mask_bits, 9, 16, 4, 1},
  {sb_h_double_arrow_bits, 15, 7, 7, 4},
  {sb_h_double_arrow_mask_bits, 15, 9, 7, 5},
  {sb_left_arrow_bits, 15, 7, -1, 4},
  {sb_left_arrow_mask_bits, 16, 9, 0, 5},
  {sb_right_arrow_bits, 15, 7, 15, 4},
  {sb_right_arrow_mask_bits, 16, 9, 15, 5},
  {sb_up_arrow_bits, 7, 15, 3, 16},
  {sb_up_arrow_mask_bits, 9, 16, 4, 16},
  {sb_v_double_arrow_bits, 7, 15, 3, 8},
  {sb_v_double_arrow_mask_bits, 9, 15, 4, 8},
  {shuttle_bits, 15, 16, 10, 16},
  {shuttle_mask_bits, 16, 16, 11, 16},
  {sizing_bits, 14, 14, 7, 7},
  {sizing_mask_bits, 16, 16, 8, 8},
  {spider_bits, 16, 16, 6, 9},
  {spider_mask_bits, 16, 16, 6, 9},
  {spraycan_bits, 11, 16, 9, 14},
  {spraycan_mask_bits, 12, 16, 10, 14},
  {star_bits, 15, 16, 7, 9},
  {star_mask_bits, 16, 16, 7, 9},
  {target_bits, 15, 13, 7, 7},
  {target_mask_bits, 16, 14, 7, 7},
  {tcross_bits, 13, 13, 6, 7},
  {tcross_mask_bits, 15, 15, 7, 8},
  {top_left_arrow_bits, 14, 14, 0, 14},
  {top_left_arrow_mask_bits, 16, 16, 1, 15},
  {top_left_corner_bits, 14, 14, 0, 14},
  {top_left_corner_mask_bits, 16, 16, 1, 15},
  {top_right_corner_bits, 14, 14, 13, 14},
  {top_right_corner_mask_bits, 16, 16, 14, 15},
  {top_side_bits, 13, 14, 6, 14},
  {top_side_mask_bits, 15, 16, 7, 15},
  {top_tee_bits, 14, 10, 7, 10},
  {top_tee_mask_bits, 16, 12, 8, 11},
  {trek_bits, 7, 16, 3, 16},
  {trek_mask_bits, 9, 16, 4, 16},
  {ul_angle_bits, 10, 10, 0, 10},
  {ul_angle_mask_bits, 12, 12, 1, 11},
  {umbrella_bits, 14, 14, 7, 12},
  {umbrella_mask_bits, 16, 16, 8, 14},
  {ur_angle_bits, 10, 10, 9, 10},
  {ur_angle_mask_bits, 12, 12, 10, 11},
  {watch_bits, 16, 16, 15, 7},
  {watch_mask_bits, 16, 16, 15, 7},
  {xterm_bits, 7, 14, 3, 7},
  {xterm_mask_bits, 9, 16, 4, 8}
};

GdkCursor*
gdk_cursor_new (GdkCursorType cursor_type)
{
  GdkPixmap *pm, *mask;

  if(cursor_type >= sizeof(stock_cursors)/sizeof(stock_cursors[0]))
    return NULL;

  pm = stock_cursors[cursor_type].pm;
  if(!pm)
    {
      pm = stock_cursors[cursor_type].pm = gdk_bitmap_create_from_data(gdk_parent_root,
								       stock_cursors[cursor_type].bits,
								       stock_cursors[cursor_type].width,
								       stock_cursors[cursor_type].height);
      gdk_pixmap_ref(pm);
    }
  mask = stock_cursors[cursor_type+1].pm;
  if(!mask)
    {
      mask = stock_cursors[cursor_type+1].pm = gdk_bitmap_create_from_data(gdk_parent_root,
									   stock_cursors[cursor_type+1].bits,
									   stock_cursors[cursor_type+1].width,
									   stock_cursors[cursor_type+1].height);
      gdk_pixmap_ref(mask);
    }

  return gdk_cursor_new_from_pixmap(pm, mask, NULL, NULL,
				    stock_cursors[cursor_type].hotx,
				    stock_cursors[cursor_type].hoty);
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap *source,
			    GdkPixmap *mask,
			    GdkColor  *fg,
			    GdkColor  *bg,
			    gint       x,
			    gint       y)
{
  GdkCursorPrivateFB *private;
  GdkCursor *cursor;

  g_return_val_if_fail (source != NULL, NULL);

  private = g_new (GdkCursorPrivateFB, 1);
  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  private->cursor = gdk_pixmap_ref(source);
  private->mask = gdk_pixmap_ref(mask);
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

  g_free (private);
}
