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

#include "gdkwin32.h"
#include "gdkcursor.h"
#include "gdkinternals.h"
#include "gdkpixmap-win32.h"
#include "gdkprivate-win32.h"

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
  HCURSOR hcursor;
  int i;

  for (i = 0; cursors[i].name != NULL && cursors[i].type != cursor_type; i++)
    ;
  if (cursors[i].name != NULL)
    {
      hcursor = LoadCursor (gdk_dll_hinstance, cursors[i].name);
      if (hcursor == NULL)
	WIN32_API_FAILED ("LoadCursor");
      GDK_NOTE (MISC, g_print ("gdk_cursor_new: %#x %d\n",
			       hcursor, cursor_type));
    }
  else
    {
      g_warning ("gdk_cursor_new: no cursor %d found",
		 cursor_type);
      hcursor = NULL;
    }

  private = g_new (GdkCursorPrivate, 1);
  private->hcursor = hcursor;
  cursor = (GdkCursor*) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;

  return cursor;
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap *source,
			    GdkPixmap *mask,
			    GdkColor  *fg,
			    GdkColor  *bg,
			    gint       x,
			    gint       y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  GdkPixmapImplWin32 *source_impl, *mask_impl;
  GdkImage *source_image, *mask_image;
  HCURSOR hcursor;
  guchar *p, *q, *xor_mask, *and_mask;
  gint width, height, cursor_width, cursor_height;
  guchar residue;
  gint ix, iy;
  
  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL);
  g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  source_impl = GDK_PIXMAP_IMPL_WIN32 (source);
  mask_impl   = GDK_PIXMAP_IMPL_WIN32 (mask);

  g_return_val_if_fail (source_impl->width == mask_impl->width
			&& source_impl->height == mask_impl->height,
			NULL);
  width = source_impl->width;
  height = source_impl->height;
  cursor_width = GetSystemMetrics (SM_CXCURSOR);
  cursor_height = GetSystemMetrics (SM_CYCURSOR);

  g_return_val_if_fail (width <= cursor_width && height <= cursor_height,
			NULL);

  residue = (1 << ((8-(width%8))%8)) - 1;

  source_image = gdk_image_get (source, 0, 0, width, height);
  mask_image = gdk_image_get (mask, 0, 0, width, height);

  if (source_image->depth != 1 || mask_image->depth != 1)
    {
      gdk_image_unref (source_image);
      gdk_image_unref (mask_image);
      g_return_val_if_fail (source_image->depth == 1 && mask_image->depth == 1,
			    NULL);
    }

  /* Such complex bit manipulation for this simple task, sigh.
   * The X cursor and Windows cursor concepts are quite different.
   * We assume here that we are always called with fg == black and
   * bg == white.
   */

  /* First set masked-out source bits, as all source bits matter on Windoze.
   * As we invert them below, they will be clear in the final xor_mask.
   */
  for (iy = 0; iy < height; iy++)
    {
      p = (guchar *) source_image->mem + iy*source_image->bpl;
      q = (guchar *) mask_image->mem + iy*mask_image->bpl;
      
      for (ix = 0; ix < ((width-1)/8+1); ix++)
	*p++ |= ~(*q++);
    }

  /* XOR mask is initialized to zero */
  xor_mask = g_malloc0 (cursor_width/8 * cursor_height);

  for (iy = 0; iy < height; iy++)
    {
      p = (guchar *) source_image->mem + iy*source_image->bpl;
      q = xor_mask + iy*cursor_width/8;

      for (ix = 0; ix < ((width-1)/8+1); ix++)
	*q++ = ~(*p++);
      q[-1] &= ~residue;	/* Clear left-over bits */
    }
      
  /* AND mask is initialized to ones */
  and_mask = g_malloc (cursor_width/8 * cursor_height);
  memset (and_mask, 0xFF, cursor_width/8 * cursor_height);

  for (iy = 0; iy < height; iy++)
    {
      p = (guchar *) mask_image->mem + iy*mask_image->bpl;
      q = and_mask + iy*cursor_width/8;

      for (ix = 0; ix < ((width-1)/8+1); ix++)
	*q++ = ~(*p++);
      q[-1] |= residue;	/* Set left-over bits */
    }
      
  hcursor = CreateCursor (gdk_app_hmodule, x, y, cursor_width, cursor_height,
			  and_mask, xor_mask);

  GDK_NOTE (MISC, g_print ("gdk_cursor_new_from_pixmap: "
			   "%#x (%dx%d) %#x (%dx%d) = %#x (%dx%d)\n",
			   GDK_PIXMAP_HBITMAP (source),
			   source_impl->width, source_impl->height,
			   GDK_PIXMAP_HBITMAP (mask),
			   mask_impl->width, mask_impl->height,
			   hcursor, cursor_width, cursor_height));

  g_free (xor_mask);
  g_free (and_mask);

  gdk_image_unref (source_image);
  gdk_image_unref (mask_image);

  private = g_new (GdkCursorPrivate, 1);
  private->hcursor = hcursor;
  cursor = (GdkCursor*) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  
  return cursor;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  private = (GdkCursorPrivate *) cursor;

  GDK_NOTE (MISC, g_print ("_gdk_cursor_destroy: %#x\n",
			   (cursor->type == GDK_CURSOR_IS_PIXMAP) ? private->hcursor : 0));

  if (cursor->type == GDK_CURSOR_IS_PIXMAP)
    if (!DestroyCursor (private->hcursor))
      WIN32_API_FAILED ("DestroyCursor");

  g_free (private);
}
