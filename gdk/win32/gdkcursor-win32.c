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

#include "gdkcursor.h"
#include "gdkwin32.h"
#include "xcursors.h"

static HCURSOR
_gdk_win32_data_to_wcursor (GdkCursorType cursor_type)
{
  gint i, j, x, y, ofs;
  HCURSOR rv = NULL;
  gint w, h;
  guchar *ANDplane, *XORplane;

  for (i = 0; i < G_N_ELEMENTS (cursors); i++)
    if (cursors[i].type == cursor_type)
      break;

  if (i >= G_N_ELEMENTS (cursors) || !cursors[i].name)
    return NULL;

  w = GetSystemMetrics (SM_CXCURSOR);
  h = GetSystemMetrics (SM_CYCURSOR);

  ANDplane = g_malloc ((w/8) * h);
  memset (ANDplane, 0xff, (w/8) * h);
  XORplane = g_malloc ((w/8) * h);
  memset (XORplane, 0, (w/8) * h);

#define SET_BIT(v,b)  (v |= (1 << b))
#define RESET_BIT(v,b)  (v &= ~(1 << b))

  for (j = 0, y = 0; y < cursors[i].height && y < h ; y++)
    {
      ofs = (y * w) / 8;
      j = y * cursors[i].width;

      for (x = 0; x < cursors[i].width && x < w ; x++, j++)
      {
        gint pofs = ofs + x / 8;
        guchar data = (cursors[i].data[j/4] & (0xc0 >> (2 * (j%4)))) >> (2 * (3 - (j%4)));
        gint bit = 7 - (j % cursors[i].width) % 8;

        if (data)
          {
            RESET_BIT (ANDplane[pofs], bit);
            if (data == 1)
              SET_BIT (XORplane[pofs], bit);
          }
      }
    }

#undef SET_BIT
#undef RESET_BIT

  rv = CreateCursor (gdk_ProgInstance, cursors[i].hotx, cursors[i].hoty,
		     w, h, ANDplane, XORplane);
  
  return rv;
}

GdkCursor*
gdk_cursor_new (GdkCursorType cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  HCURSOR xcursor;

  xcursor = _gdk_win32_data_to_wcursor (cursor_type);

  if (xcursor == NULL)
    g_warning ("gdk_cursor_new: no cursor %d found", cursor_type);
  else
    GDK_NOTE (MISC, g_print ("gdk_cursor_new: %d: %#x\n", cursor_type, xcursor));

  private = g_new (GdkCursorPrivate, 1);
  private->xcursor = xcursor;
  cursor = (GdkCursor*) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;

  return cursor;
}

static gboolean
color_is_white (GdkColor *color)
{
  return (color->red == 0xFFFF
	  && color->green == 0xFFFF
	  && color->blue == 0xFFFF);
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
  GdkDrawablePrivate *source_private, *mask_private;
  GdkImage *source_image, *mask_image;
  HCURSOR xcursor;
  guchar *p, *q, *XORmask, *ANDmask;
  gint width, height, cursor_width, cursor_height;
  guchar residue;
  gint ix, iy;
  gboolean bg_is_white = color_is_white (bg);
  
  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (mask != NULL, NULL);

  source_private = (GdkDrawablePrivate *) source;
  mask_private   = (GdkDrawablePrivate *) mask;

  g_return_val_if_fail (source_private->width == mask_private->width
			&& source_private->height == mask_private->height,
			NULL);
  width = source_private->width;
  height = source_private->height;
  cursor_width = GetSystemMetrics (SM_CXCURSOR);
  cursor_height = GetSystemMetrics (SM_CYCURSOR);

  g_return_val_if_fail (width <= cursor_width
			&& height <= cursor_height, NULL);

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
   * bg == white, *or* the other way around. Random colours won't work.
   * (Well, you will get a cursor, but not in those colours.)
   */

  /* Note: The comments below refer to the case fg==black and
   * bg==white.
   */

  /* First set masked-out source bits, as all source bits matter on Windoze.
   * As we invert them below, they will be clear in the final XORmask.
   */
  for (iy = 0; iy < height; iy++)
    {
      p = (guchar *) source_image->mem + iy*source_image->bpl;
      q = (guchar *) mask_image->mem + iy*mask_image->bpl;
      
      for (ix = 0; ix < ((width-1)/8+1); ix++)
	if (bg_is_white)
	  *p++ |= ~(*q++);
	else
	  *p++ &= *q++;
    }

  /* XOR mask is initialized to zero */
  XORmask = g_malloc0 (cursor_width/8 * cursor_height);

  for (iy = 0; iy < height; iy++)
    {
      p = (guchar *) source_image->mem + iy*source_image->bpl;
      q = XORmask + iy*cursor_width/8;

      for (ix = 0; ix < ((width-1)/8+1); ix++)
	if (bg_is_white)
	  *q++ = ~(*p++);
	else
	  *q++ = *p++;
      q[-1] &= ~residue;	/* Clear left-over bits */
    }
      
  /* AND mask is initialized to ones */
  ANDmask = g_malloc (cursor_width/8 * cursor_height);
  memset (ANDmask, 0xFF, cursor_width/8 * cursor_height);

  for (iy = 0; iy < height; iy++)
    {
      p = (guchar *) mask_image->mem + iy*mask_image->bpl;
      q = ANDmask + iy*cursor_width/8;

      for (ix = 0; ix < ((width-1)/8+1); ix++)
	*q++ = ~(*p++);
      q[-1] |= residue;	/* Set left-over bits */
    }
      
  xcursor = CreateCursor (gdk_ProgInstance, x, y, cursor_width, cursor_height,
			  ANDmask, XORmask);

  GDK_NOTE (MISC, g_print ("gdk_cursor_new_from_pixmap: "
			   "%#x (%dx%d) %#x (%dx%d) = %#x (%dx%d)\n",
			   GDK_DRAWABLE_XID (source),
			   source_private->width, source_private->height,
			   GDK_DRAWABLE_XID (mask),
			   mask_private->width, mask_private->height,
			   xcursor, cursor_width, cursor_height));

  g_free (XORmask);
  g_free (ANDmask);

  gdk_image_unref (source_image);
  gdk_image_unref (mask_image);

  private = g_new (GdkCursorPrivate, 1);
  private->xcursor = xcursor;
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

  GDK_NOTE (MISC, g_print ("_gdk_cursor_destroy: %#x\n", private->xcursor));

  if (!DestroyCursor (private->xcursor))
    WIN32_API_FAILED ("DestroyCursor");

  g_free (private);
}
