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

#include "config.h"

#include <gdk/gdk.h>
#include "gdkprivate.h"

static const struct { const char *name; int type; } cursors[] = {
  { "x_cursor", 0 },
  { "arrow", 2 },
  { "based_arrow_down", 4 },
  { "based_arrow_up", 6 },
  { "boat", 8 },
  { "bogosity", 10 },
  { "bottom_left_corner", 12 },
  { "bottom_right_corner", 14 },
  { "bottom_side", 16 },
  { "bottom_tee", 18 },
  { "box_spiral", 20 },
  { "center_ptr", 22 },
  { "circle", 24 },
  { "clock", 26 },
  { "coffee_mug", 28 },
  { "cross", 30 },
  { "cross_reverse", 32 },
  { "crosshair", 34 },
  { "diamond_cross", 36 },
  { "dot", 38 },
  { "dotbox", 40 },
  { "double_arrow", 42 },
  { "draft_large", 44 },
  { "draft_small", 46 },
  { "draped_box", 48 },
  { "exchange", 50 },
  { "fleur", 52 },
  { "gobbler", 54 },
  { "gumby", 56 },
  { "hand1", 58 },
  { "hand2", 60 },
  { "heart", 62 },
  { "icon", 64 },
  { "iron_cross", 66 },
  { "left_ptr", 68 },
  { "left_side", 70 },
  { "left_tee", 72 },
  { "leftbutton", 74 },
  { "ll_angle", 76 },
  { "lr_angle", 78 },
  { "man", 80 },
  { "middlebutton", 82 },
  { "mouse", 84 },
  { "pencil", 86 },
  { "pirate", 88 },
  { "plus", 90 },
  { "question_arrow", 92 },
  { "right_ptr", 94 },
  { "right_side", 96 },
  { "right_tee", 98 },
  { "rightbutton", 100 },
  { "rtl_logo", 102 },
  { "sailboat", 104 },
  { "sb_down_arrow", 106 },
  { "sb_h_double_arrow", 108 },
  { "sb_left_arrow", 110 },
  { "sb_right_arrow", 112 },
  { "sb_up_arrow", 114 },
  { "sb_v_double_arrow", 116 },
  { "shuttle", 118 },
  { "sizing", 120 },
  { "spider", 122 },
  { "spraycan", 124 },
  { "star", 126 },
  { "target", 128 },
  { "tcross", 130 },
  { "top_left_arrow", 132 },
  { "top_left_corner", 134 },
  { "top_right_corner", 136 },
  { "top_side", 138 },
  { "top_tee", 140 },
  { "trek", 142 },
  { "ul_angle", 144 },
  { "umbrella", 146 },
  { "ur_angle", 148 },
  { "watch", 150 },
  { "xterm", 152 },
  { NULL, 0 }
};  

GdkCursor*
gdk_cursor_new (GdkCursorType cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  HCURSOR xcursor;

  int i;

  for (i = 0; cursors[i].name != NULL && cursors[i].type != cursor_type; i++)
    ;
  if (cursors[i].name != NULL)
    {
      xcursor = LoadCursor (gdk_DLLInstance, cursors[i].name);
      if (xcursor == NULL)
	g_warning ("gdk_cursor_new: LoadCursor failed");
      GDK_NOTE (MISC, g_print ("gdk_cursor_new: %#x %d\n",
			       xcursor, cursor_type));
    }
  else
    {
      g_warning ("gdk_cursor_new: no cursor %d found",
		 cursor_type);
      xcursor = NULL;
    }

  private = g_new (GdkCursorPrivate, 1);
  private->xcursor = xcursor;
  cursor = (GdkCursor*) private;
  cursor->type = cursor_type;

  return cursor;
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap *source, GdkPixmap *mask, GdkColor *fg, GdkColor *bg, gint x, gint y)
{
#if 0				/* I don't understand cursors, sigh */
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  GdkPixmap *s2;
  GdkPixmapPrivate *source_private, *mask_private;
  GdkPixmapPrivate *s2_private;
  GdkGC *gc;
  ICONINFO iconinfo;
  HCURSOR xcursor;
  HBITMAP invmask;
  HDC hdc1, hdc2;
  HGDIOBJ oldbm1, oldbm2;
  
  source_private = (GdkPixmapPrivate *) source;
  mask_private   = (GdkPixmapPrivate *) mask;

  s2 = gdk_pixmap_new (source, source_private->width, source_private->height, 1);
  gc = gdk_gc_new (s2);
  gdk_gc_set_foreground (gc, fg);
  gdk_gc_set_background (gc, bg);
  gdk_draw_pixmap (s2, gc, source, 0, 0, 0, 0,
		   source_private->width, source_private->height);
  gdk_gc_unref (gc);
  
  iconinfo.fIcon = FALSE;
  iconinfo.xHotspot = x;
  iconinfo.yHotspot = y;
#if 1
  invmask = CreateBitmap (mask_private->width, mask_private->height, 1, 1, NULL);
  hdc1 = CreateCompatibleDC (gdk_DC);
  oldbm1 = SelectObject (hdc1, invmask);
  hdc2 = CreateCompatibleDC (gdk_DC);
  oldbm2 = SelectObject (hdc2, mask_private->xwindow);
  BitBlt (hdc1, 0, 0, mask_private->width, mask_private->height, hdc2, 0, 0, NOTSRCCOPY);
  SelectObject (hdc2, oldbm2);
  DeleteDC (hdc2);
  SelectObject (hdc1, oldbm1);
  DeleteDC (hdc1);
  iconinfo.hbmMask = invmask;
#else
  iconinfo.hbmMask = mask_private->xwindow;;
#endif
  iconinfo.hbmColor = ((GdkPixmapPrivate *) s2)->xwindow;

  if ((xcursor = CreateIconIndirect (&iconinfo)) == NULL)
    {
      g_warning ("gdk_cursor_new_from_private: CreateIconIndirect failed");
      gdk_pixmap_unref (s2);
      return gdk_cursor_new (GDK_PIRATE);
    }

  GDK_NOTE (MISC,
	    g_print ("gdk_cursor_new_from_private: %#x (%dx%d) %#x (%dx%d) = %#x\n",
		     source_private->xwindow,
		     source_private->width, source_private->height,
		     mask_private->xwindow,
		     mask_private->width, mask_private->height,
		     xcursor));

  gdk_pixmap_unref (s2);
  private = g_new (GdkCursorPrivate, 1);
  private->xcursor = xcursor;
  cursor = (GdkCursor*) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;

  return cursor;
#else  /* Just return some cursor ;-) */
  return gdk_cursor_new (GDK_PIRATE);
#endif
}

void
gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  private = (GdkCursorPrivate *) cursor;

  if (cursor->type == GDK_CURSOR_IS_PIXMAP)
    DestroyIcon (private->xcursor);

  g_free (private);
}
