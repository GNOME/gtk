/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include <config.h>
#include "gdkdisplay.h"
#include "gdkscreen.h"
#include "gdkcursor.h"
#include "gdkprivate-win32.h"

#include "xcursors.h"

static HCURSOR
_gdk_win32_data_to_wcursor (GdkCursorType cursor_type)
{
  gint i, j, x, y, ofs;
  HCURSOR rv = NULL;
  gint w, h;
  guchar *and_plane, *xor_plane;

  for (i = 0; i < G_N_ELEMENTS (cursors); i++)
    if (cursors[i].type == cursor_type)
      break;

  if (i >= G_N_ELEMENTS (cursors) || !cursors[i].name)
    return NULL;

  w = GetSystemMetrics (SM_CXCURSOR);
  h = GetSystemMetrics (SM_CYCURSOR);

  and_plane = g_malloc ((w/8) * h);
  memset (and_plane, 0xff, (w/8) * h);
  xor_plane = g_malloc ((w/8) * h);
  memset (xor_plane, 0, (w/8) * h);

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
            RESET_BIT (and_plane[pofs], bit);
            if (data == 1)
              SET_BIT (xor_plane[pofs], bit);
          }
      }
    }

#undef SET_BIT
#undef RESET_BIT

  rv = CreateCursor (_gdk_app_hmodule, cursors[i].hotx, cursors[i].hoty,
		     w, h, and_plane, xor_plane);
  if (rv == NULL)
    WIN32_API_FAILED ("CreateCursor");
  g_free (and_plane);
  g_free (xor_plane);
  
  return rv;
}

GdkCursor*
gdk_cursor_new_for_display (GdkDisplay   *display,
			    GdkCursorType cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  HCURSOR hcursor;

  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  hcursor = _gdk_win32_data_to_wcursor (cursor_type);

  if (hcursor == NULL)
    g_warning ("gdk_cursor_new_for_display: no cursor %d found", cursor_type);
  else
    GDK_NOTE (MISC, g_print ("gdk_cursor_new_for_display: %d: %p\n",
			     cursor_type, hcursor));

  private = g_new (GdkCursorPrivate, 1);
  private->hcursor = hcursor;
  cursor = (GdkCursor*) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;

  return cursor;
}

static gboolean
color_is_white (const GdkColor *color)
{
  return (color->red == 0xFFFF
	  && color->green == 0xFFFF
	  && color->blue == 0xFFFF);
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap      *source,
			    GdkPixmap      *mask,
			    const GdkColor *fg,
			    const GdkColor *bg,
			    gint            x,
			    gint            y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  GdkPixmapImplWin32 *source_impl, *mask_impl;
  guchar *source_bits, *mask_bits;
  gint source_bpl, mask_bpl;
  HCURSOR hcursor;
  guchar *p, *q, *xor_mask, *and_mask;
  gint width, height, cursor_width, cursor_height;
  guchar residue;
  gint ix, iy;
  const gboolean bg_is_white = color_is_white (bg);
  
  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL);
  g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  source_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (source)->impl);
  mask_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (mask)->impl);

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

  source_bits = source_impl->bits;
  mask_bits = mask_impl->bits;

  g_return_val_if_fail (GDK_PIXMAP_OBJECT (source)->depth == 1
  			&& GDK_PIXMAP_OBJECT (mask)->depth == 1,
			NULL);

  source_bpl = ((width - 1)/32 + 1)*4;
  mask_bpl = ((mask_impl->width - 1)/32 + 1)*4;

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_CURSOR)
    {
      g_print ("gdk_cursor_new_from_pixmap: source=%p:\n",
	       source_impl->parent_instance.handle);
      for (iy = 0; iy < height; iy++)
	{
	  if (iy == 16)
	    break;

	  p = source_bits + iy*source_bpl;
	  for (ix = 0; ix < width; ix++)
	    {
	      if (ix == 79)
		break;
	      g_print ("%c", ".X"[((*p)>>(7-(ix%8)))&1]);
	      if ((ix%8) == 7)
		p++;
	    }
	  g_print ("\n");
	}
      g_print ("...mask=%p:\n", mask_impl->parent_instance.handle);
      for (iy = 0; iy < height; iy++)
	{
	  if (iy == 16)
	    break;

	  p = mask_bits + iy*source_bpl;
	  for (ix = 0; ix < width; ix++)
	    {
	      if (ix == 79)
		break;
	      g_print ("%c", ".X"[((*p)>>(7-(ix%8)))&1]);
	      if ((ix%8) == 7)
		p++;
	    }
	  g_print ("\n");
	}
    }
#endif

  /* Such complex bit manipulation for this simple task, sigh.
   * The X cursor and Windows cursor concepts are quite different.
   * We assume here that we are always called with fg == black and
   * bg == white, *or* the other way around. Random colours won't work.
   * (Well, you will get a cursor, but not in those colours.)
   */

  /* Note: The comments below refer to the case fg==black and
   * bg==white, as that was what was implemented first. The fg==white
   * (the "if (fg->pixel)" branches) case was added later.
   */

  /* First set masked-out source bits, as all source bits matter on Windoze.
   * As we invert them below, they will be clear in the final xor_mask.
   */
  for (iy = 0; iy < height; iy++)
    {
      p = source_bits + iy*source_bpl;
      q = mask_bits + iy*mask_bpl;
      
      for (ix = 0; ix < ((width-1)/8+1); ix++)
	if (bg_is_white)
	  *p++ |= ~(*q++);
	else
	  *p++ &= *q++;
    }

  /* XOR mask is initialized to zero */
  xor_mask = g_malloc0 (cursor_width/8 * cursor_height);

  for (iy = 0; iy < height; iy++)
    {
      p = source_bits + iy*source_bpl;
      q = xor_mask + iy*cursor_width/8;

      for (ix = 0; ix < ((width-1)/8+1); ix++)
	if (bg_is_white)
	  *q++ = ~(*p++);
	else
	  *q++ = *p++;

      q[-1] &= ~residue;	/* Clear left-over bits */
    }
      
  /* AND mask is initialized to ones */
  and_mask = g_malloc (cursor_width/8 * cursor_height);
  memset (and_mask, 0xFF, cursor_width/8 * cursor_height);

  for (iy = 0; iy < height; iy++)
    {
      p = mask_bits + iy*mask_bpl;
      q = and_mask + iy*cursor_width/8;

      for (ix = 0; ix < ((width-1)/8+1); ix++)
	*q++ = ~(*p++);

      q[-1] |= residue;	/* Set left-over bits */
    }
      
  hcursor = CreateCursor (_gdk_app_hmodule, x, y, cursor_width, cursor_height,
			  and_mask, xor_mask);

  GDK_NOTE (MISC, g_print ("gdk_cursor_new_from_pixmap: "
			   "%p (%dx%d) %p (%dx%d) = %p (%dx%d)\n",
			   GDK_PIXMAP_HBITMAP (source),
			   source_impl->width, source_impl->height,
			   GDK_PIXMAP_HBITMAP (mask),
			   mask_impl->width, mask_impl->height,
			   hcursor, cursor_width, cursor_height));

  g_free (xor_mask);
  g_free (and_mask);

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

  GDK_NOTE (MISC, g_print ("_gdk_cursor_destroy: %p\n",
			   (cursor->type == GDK_CURSOR_IS_PIXMAP) ? private->hcursor : 0));

  if (GetCursor () == private->hcursor)
    SetCursor (NULL);

  if (!DestroyCursor (private->hcursor))
    WIN32_API_FAILED ("DestroyCursor");

  g_free (private);
}

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  return gdk_display_get_default ();
}

GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display, 
			    GdkPixbuf  *pixbuf,
			    gint        x,
			    gint        y)
{
  /* FIXME: Use alpha if supported */

  GdkCursor *cursor;
  GdkPixmap *pixmap, *mask;
  guint width, height, n_channels, rowstride, i, j;
  guint8 *data, *mask_data, *pixels;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0xffff, 0xffff, 0xffff };
  GdkScreen *screen;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  g_return_val_if_fail (0 <= x && x < width, NULL);
  g_return_val_if_fail (0 <= y && y < height, NULL);

  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  data = g_new0 (guint8, (width + 7) / 8 * height);
  mask_data = g_new0 (guint8, (width + 7) / 8 * height);

  for (j = 0; j < height; j++)
    {
      guint8 *src = pixels + j * rowstride;
      guint8 *d = data + (width + 7) / 8 * j;
      guint8 *md = mask_data + (width + 7) / 8 * j;
	
      for (i = 0; i < width; i++)
	{
	  if (src[1] < 0x80)
	    *d |= 1 << (i % 8);
	  
	  if (n_channels == 3 || src[3] >= 0x80)
	    *md |= 1 << (i % 8);
	  
	  src += n_channels;
	  if (i % 8 == 7)
	    {
	      d++; 
	      md++;
	    }
	}
    }
      
  screen = gdk_display_get_default_screen (display);
  pixmap = gdk_bitmap_create_from_data (gdk_screen_get_root_window (screen), 
					data, width, height);
 
  mask = gdk_bitmap_create_from_data (gdk_screen_get_root_window (screen),
				      mask_data, width, height);
   
  cursor = gdk_cursor_new_from_pixmap (pixmap, mask, &fg, &bg, x, y);
   
  g_object_unref (pixmap);
  g_object_unref (mask);

  g_free (data);
  g_free (mask_data);
  
  return cursor;
}

gboolean 
gdk_display_supports_cursor_alpha (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* FIXME */
  return FALSE;
}

gboolean 
gdk_display_supports_cursor_color (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* FIXME */
  return FALSE;
}

guint     
gdk_display_get_default_cursor_size (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  
  return MIN (GetSystemMetrics (SM_CXCURSOR), GetSystemMetrics (SM_CYCURSOR));
}

void     
gdk_display_get_maximal_cursor_size (GdkDisplay *display,
				     guint       *width,
				     guint       *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  
  if (width)
    *width = GetSystemMetrics (SM_CXCURSOR);
  if (height)
    *height = GetSystemMetrics (SM_CYCURSOR);
}
