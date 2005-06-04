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

#if defined(__MINGW32__) || (defined(_MSC_VER) && (WINVER < 0x0500))
typedef struct { 
  DWORD        bV5Size; 
  LONG         bV5Width; 
  LONG         bV5Height; 
  WORD         bV5Planes; 
  WORD         bV5BitCount; 
  DWORD        bV5Compression; 
  DWORD        bV5SizeImage; 
  LONG         bV5XPelsPerMeter; 
  LONG         bV5YPelsPerMeter; 
  DWORD        bV5ClrUsed; 
  DWORD        bV5ClrImportant; 
  DWORD        bV5RedMask; 
  DWORD        bV5GreenMask; 
  DWORD        bV5BlueMask; 
  DWORD        bV5AlphaMask; 
  DWORD        bV5CSType; 
  CIEXYZTRIPLE bV5Endpoints; 
  DWORD        bV5GammaRed; 
  DWORD        bV5GammaGreen; 
  DWORD        bV5GammaBlue; 
  DWORD        bV5Intent; 
  DWORD        bV5ProfileData; 
  DWORD        bV5ProfileSize; 
  DWORD        bV5Reserved; 
} BITMAPV5HEADER;
#endif

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

static GdkCursor*
_gdk_win32_cursor_new_from_hcursor (HCURSOR hcursor, GdkCursorType cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;

  private = g_new (GdkCursorPrivate, 1);
  private->hcursor = hcursor;
  cursor = (GdkCursor*) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;

  return cursor;
}

GdkCursor*
gdk_cursor_new_for_display (GdkDisplay   *display,
			    GdkCursorType cursor_type)
{
  HCURSOR hcursor;

  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  hcursor = _gdk_win32_data_to_wcursor (cursor_type);

  if (hcursor == NULL)
    g_warning ("gdk_cursor_new_for_display: no cursor %d found", cursor_type);
  else
    GDK_NOTE (MISC, g_print ("gdk_cursor_new_for_display: %d: %p\n",
			     cursor_type, hcursor));

  return _gdk_win32_cursor_new_from_hcursor (hcursor, cursor_type);
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

  return _gdk_win32_cursor_new_from_hcursor (hcursor, GDK_CURSOR_IS_PIXMAP);
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
  HCURSOR hcursor;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  hcursor = _gdk_win32_pixbuf_to_hcursor (pixbuf, x, y);
  if (!hcursor)
    return NULL;
  return _gdk_win32_cursor_new_from_hcursor (hcursor, GDK_CURSOR_IS_PIXMAP);
}

gboolean 
gdk_display_supports_cursor_alpha (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return _gdk_win32_pixbuf_to_hicon_supports_alpha ();
}

gboolean 
gdk_display_supports_cursor_color (GdkDisplay    *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
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


/* Convert a pixbuf to an HICON (or HCURSOR).  Supports alpha under
 * Windows XP, thresholds alpha otherwise.  Also used from
 * gdkwindow-win32.c for creating application icons.
 */

static HBITMAP
create_alpha_bitmap (gint width, gint height, guchar **outdata)
{
  BITMAPV5HEADER bi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bi, sizeof (BITMAPV5HEADER));
  bi.bV5Size = sizeof (BITMAPV5HEADER);
  bi.bV5Width = width;
  bi.bV5Height = height;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  /* The following mask specification specifies a supported 32 BPP
   * alpha format for Windows XP (BGRA format).
   */
  bi.bV5RedMask   = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask  = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  /* Create the DIB section with an alpha channel. */
  hdc = GetDC (NULL);
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
			      (PVOID *) outdata, NULL, (DWORD)0);
  if (hBitmap == NULL)
    WIN32_GDI_FAILED ("CreateDIBSection");
  ReleaseDC (NULL, hdc);

  return hBitmap;
}

static HBITMAP
create_color_bitmap (gint width, gint height, guchar **outdata)
{
  BITMAPV4HEADER bi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bi, sizeof (BITMAPV4HEADER));
  bi.bV4Size = sizeof (BITMAPV4HEADER);
  bi.bV4Width = width;
  bi.bV4Height = height;
  bi.bV4Planes = 1;
  bi.bV4BitCount = 24;
  bi.bV4V4Compression = BI_RGB;

  hdc = GetDC (NULL);
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
			      (PVOID *) outdata, NULL, (DWORD)0);
  if (hBitmap == NULL)
    WIN32_GDI_FAILED ("CreateDIBSection");
  ReleaseDC (NULL, hdc);

  return hBitmap;
}

static gboolean
pixbuf_to_hbitmaps_alpha_winxp (GdkPixbuf *pixbuf,
				HBITMAP   *color,
				HBITMAP   *mask)
{
  /* Based on code from
   * http://www.dotnet247.com/247reference/msgs/13/66301.aspx
   */
  HBITMAP hColorBitmap, hMaskBitmap;
  guchar *indata, *inrow;
  guchar *outdata, *outrow;
  gint width, height, i, j, rowstride;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  hColorBitmap = create_alpha_bitmap (width, height, &outdata);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap = CreateBitmap (width, height, 1, 1, NULL);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* rows are always aligned on 4-byte boundarys, but here our pixels are always 4 bytes */
  indata = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  for (j=0; j<height; j++)
    {
      outrow = outdata + 4*j*width;
      inrow  = indata  + (height-j-1)*rowstride;
      for (i=0; i<width; i++)
	{
	  outrow[4*i+0] = inrow[4*i+2];
	  outrow[4*i+1] = inrow[4*i+1];
	  outrow[4*i+2] = inrow[4*i+0];
	  outrow[4*i+3] = inrow[4*i+3];
	}
    }

  if (color) *color = hColorBitmap;
  if (mask) *mask = hMaskBitmap;

  return TRUE;
}

static gboolean
pixbuf_to_hbitmaps_normal (GdkPixbuf *pixbuf,
			   HBITMAP   *color,
			   HBITMAP   *mask)
{
  /* Based on code from
   * http://www.dotnet247.com/247reference/msgs/13/66301.aspx
   */
  HBITMAP hColorBitmap, hMaskBitmap;
  guchar *indata, *inrow;
  guchar *colordata, *colorrow, *maskdata, *maskrow;
  gint width, height, i, j, rowstride, nc, bmstride;
  gboolean has_alpha;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  hColorBitmap = create_color_bitmap (width, height, &colordata);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap  = create_color_bitmap (width, height, &maskdata);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* rows are always aligned on 4-byte boundarys */
  bmstride = width * 3;
  if (bmstride % 4 != 0)
    bmstride += 4 - (bmstride % 4);

  indata = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  nc = gdk_pixbuf_get_n_channels (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  for (j=0; j<height; j++)
    {
      colorrow = colordata + j*bmstride;
      maskrow = maskdata + j*bmstride;
      inrow  = indata  + (height-j-1)*rowstride;
      for (i=0; i<width; i++)
	{
	  if (has_alpha && inrow[nc*i+3] < 128)
	    {
	      colorrow[3*i+0] = colorrow[3*i+1] = colorrow[3*i+2] = 0;
	      maskrow[3*i+0] = maskrow[3*i+1] = maskrow[3*i+2] = 255;
	    }
	  else
	    {
	      colorrow[3*i+0] = inrow[nc*i+2];
	      colorrow[3*i+1] = inrow[nc*i+1];
	      colorrow[3*i+2] = inrow[nc*i+0];
	      maskrow[3*i+0] = maskrow[3*i+1] = maskrow[3*i+2] = 0;
	    }
	}
    }

  if (color) *color = hColorBitmap;
  if (mask) *mask = hMaskBitmap;

  return TRUE;
}

static HICON
pixbuf_to_hicon (GdkPixbuf *pixbuf,
		 gboolean   is_icon,
		 gint       x,
		 gint       y)
{
  ICONINFO ii;
  HICON icon;
  gboolean success;

  if (pixbuf == NULL)
    return NULL;

  if (_gdk_win32_pixbuf_to_hicon_supports_alpha() && gdk_pixbuf_get_has_alpha (pixbuf))
    success = pixbuf_to_hbitmaps_alpha_winxp (pixbuf, &ii.hbmColor, &ii.hbmMask);
  else
    success = pixbuf_to_hbitmaps_normal (pixbuf, &ii.hbmColor, &ii.hbmMask);

  if (!success)
    return NULL;

  ii.fIcon = is_icon;
  ii.xHotspot = x;
  ii.yHotspot = y;
  icon = CreateIconIndirect (&ii);
  DeleteObject (ii.hbmColor);
  DeleteObject (ii.hbmMask);
  return icon;
}

HICON
_gdk_win32_pixbuf_to_hicon (GdkPixbuf *pixbuf)
{
  return pixbuf_to_hicon (pixbuf, TRUE, 0, 0);
}

HICON
_gdk_win32_pixbuf_to_hcursor (GdkPixbuf *pixbuf,
			      gint       x_hotspot,
			      gint       y_hotspot)
{
  return pixbuf_to_hicon (pixbuf, FALSE, x_hotspot, y_hotspot);
}

gboolean
_gdk_win32_pixbuf_to_hicon_supports_alpha (void)
{
  static gboolean is_win_xp=FALSE, is_win_xp_checked=FALSE;

  if (!is_win_xp_checked)
    {
      OSVERSIONINFO version;

      is_win_xp_checked = TRUE;
      memset (&version, 0, sizeof (version));
      version.dwOSVersionInfoSize = sizeof (version);
      is_win_xp = GetVersionEx (&version)
	&& version.dwPlatformId == VER_PLATFORM_WIN32_NT
	&& (version.dwMajorVersion > 5
	    || (version.dwMajorVersion == 5 && version.dwMinorVersion >= 1));
    }
  return is_win_xp;
}
