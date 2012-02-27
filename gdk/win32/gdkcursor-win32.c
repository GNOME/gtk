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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#define GDK_PIXBUF_ENABLE_BACKEND /* Ugly? */
#include "gdkdisplay.h"
#include "gdkscreen.h"
#include "gdkcursor.h"
#include "gdkprivate-win32.h"
#include "gdkwin32cursor.h"

#ifdef __MINGW32__
#include <w32api.h>
#endif

#include "xcursors.h"

#if (defined(__MINGW32__) && (__W32API_MAJOR_VERSION < 3 || (__W32API_MAJOR_VERSION == 3 && __W32API_MINOR_VERSION < 8))) || (defined(_MSC_VER) && (WINVER < 0x0500))
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
hcursor_from_type (GdkCursorType cursor_type)
{
  gint i, j, x, y, ofs;
  HCURSOR rv;
  gint w, h;
  guchar *and_plane, *xor_plane;

  if (cursor_type != GDK_BLANK_CURSOR)
    {
      for (i = 0; i < G_N_ELEMENTS (cursors); i++)
	if (cursors[i].type == cursor_type)
	  break;

      if (i >= G_N_ELEMENTS (cursors) || !cursors[i].name)
	return NULL;

      /* Use real Win32 cursor if possible */
      if (cursors[i].builtin)
	return LoadCursor (NULL, cursors[i].builtin);
    }
  
  w = GetSystemMetrics (SM_CXCURSOR);
  h = GetSystemMetrics (SM_CYCURSOR);

  and_plane = g_malloc ((w/8) * h);
  memset (and_plane, 0xff, (w/8) * h);
  xor_plane = g_malloc ((w/8) * h);
  memset (xor_plane, 0, (w/8) * h);

  if (cursor_type != GDK_BLANK_CURSOR)
    {

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
    }
  else
    {
      rv = CreateCursor (_gdk_app_hmodule, 0, 0,
			 w, h, and_plane, xor_plane);
    }
  if (rv == NULL)
    WIN32_API_FAILED ("CreateCursor");
  g_free (and_plane);
  g_free (xor_plane);
  
  return rv;
}

struct _GdkWin32CursorClass
{
  GdkCursorClass cursor_class;
};

G_DEFINE_TYPE (GdkWin32Cursor, gdk_win32_cursor, GDK_TYPE_CURSOR)

static void
_gdk_win32_cursor_finalize (GObject *object)
{
  GdkWin32Cursor *private = GDK_WIN32_CURSOR (object);

  if (GetCursor () == private->hcursor)
    SetCursor (NULL);

  if (!DestroyCursor (private->hcursor))
    WIN32_API_FAILED ("DestroyCursor");

  G_OBJECT_CLASS (gdk_win32_cursor_parent_class)->finalize (object);
}

static GdkCursor*
cursor_new_from_hcursor (HCURSOR       hcursor,
			 GdkCursorType cursor_type)
{
  GdkWin32Cursor *private;
  GdkCursor *cursor;

  private = g_object_new (GDK_TYPE_WIN32_CURSOR,
                          "cursor-type", cursor_type,
                          "display", _gdk_display,
			  NULL);
  private->hcursor = hcursor;
  cursor = (GdkCursor*) private;

  return cursor;
}

GdkCursor*
_gdk_win32_display_get_cursor_for_type (GdkDisplay   *display,
					GdkCursorType cursor_type)
{
  HCURSOR hcursor;

  g_return_val_if_fail (display == _gdk_display, NULL);

  hcursor = hcursor_from_type (cursor_type);

  if (hcursor == NULL)
    g_warning ("gdk_cursor_new_for_display: no cursor %d found", cursor_type);
  else
    GDK_NOTE (CURSOR, g_print ("gdk_cursor_new_for_display: %d: %p\n",
			       cursor_type, hcursor));

  return cursor_new_from_hcursor (hcursor, cursor_type);
}

/* FIXME: The named cursors below are presumably not really useful, as
 * the names are Win32-specific. No GTK+ application developed on Unix
 * (and most cross-platform GTK+ apps are developed on Unix) is going
 * to look for cursors under these Win32 names anyway.
 *
 * Would the following make any sense: The ms-windows theme engine
 * calls some (to-be-defined private) API here in gdk/win32 to
 * register the relevant cursors used by the currently active XP
 * visual style under the names that libgtk uses to look for them
 * ("color-picker", "dnd-ask", "dnd-copy", etc), and then when libgtk
 * asks for those we return the ones registered by the ms-windows
 * theme engine, if any.
 */

static struct {
  char *name;
  char *id;
} default_cursors[] = {
  { "appstarting", IDC_APPSTARTING },
  { "arrow", IDC_ARROW },
  { "cross", IDC_CROSS },
#ifdef IDC_HAND
  { "hand",  IDC_HAND },
#endif
  { "help",  IDC_HELP },
  { "ibeam", IDC_IBEAM },
  { "sizeall", IDC_SIZEALL },
  { "sizenesw", IDC_SIZENESW },
  { "sizens", IDC_SIZENS },
  { "sizenwse", IDC_SIZENWSE },
  { "sizewe", IDC_SIZEWE },
  { "uparrow", IDC_UPARROW },
  { "wait", IDC_WAIT }
};

GdkCursor*  
_gdk_win32_display_get_cursor_for_name (GdkDisplay  *display,
				        const gchar *name)
{
  HCURSOR hcursor = NULL;
  int i;

  g_return_val_if_fail (display == _gdk_display, NULL);

  for (i = 0; i < G_N_ELEMENTS(default_cursors); i++)
    {
      if (0 == strcmp(default_cursors[i].name, name))
        hcursor = LoadCursor (NULL, default_cursors[i].id);
    }
  /* allow to load named cursor resources linked into the executable */
  if (!hcursor)
    hcursor = LoadCursor (_gdk_app_hmodule, name);

  if (hcursor)
    return cursor_new_from_hcursor (hcursor, GDK_X_CURSOR);

  return NULL;
}

GdkPixbuf *
gdk_win32_icon_to_pixbuf_libgtk_only (HICON hicon)
{
  GdkPixbuf *pixbuf = NULL;
  ICONINFO ii;
  struct
  {
    BITMAPINFOHEADER bi;
    RGBQUAD colors[2];
  } bmi;
  HDC hdc;
  guchar *pixels, *bits;
  gchar buf[32];
  gint rowstride, x, y, w, h;

  if (!GDI_CALL (GetIconInfo, (hicon, &ii)))
    return NULL;

  if (!(hdc = CreateCompatibleDC (NULL)))
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      goto out0;
    }

  memset (&bmi, 0, sizeof (bmi));
  bmi.bi.biSize = sizeof (bmi.bi);

  if (ii.hbmColor != NULL)
    {
      /* Colour cursor */

      gboolean no_alpha;
      
      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmColor, 0, 1, NULL, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out1;

      w = bmi.bi.biWidth;
      h = bmi.bi.biHeight;

      bmi.bi.biBitCount = 32;
      bmi.bi.biCompression = BI_RGB;
      bmi.bi.biHeight = -h;

      bits = g_malloc0 (4 * w * h);
      
      /* color data */
      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmColor, 0, h, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out2;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);
      pixels = gdk_pixbuf_get_pixels (pixbuf);
      rowstride = gdk_pixbuf_get_rowstride (pixbuf);
      no_alpha = TRUE;
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      pixels[2] = bits[(x+y*w) * 4];
	      pixels[1] = bits[(x+y*w) * 4 + 1];
	      pixels[0] = bits[(x+y*w) * 4 + 2];
	      pixels[3] = bits[(x+y*w) * 4 + 3];
	      if (no_alpha && pixels[3] > 0)
		no_alpha = FALSE;
	      pixels += 4;
	    }
	  pixels += (w * 4 - rowstride);
	}

      /* mask */
      if (no_alpha &&
	  GDI_CALL (GetDIBits, (hdc, ii.hbmMask, 0, h, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	{
	  pixels = gdk_pixbuf_get_pixels (pixbuf);
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  pixels[3] = 255 - bits[(x + y * w) * 4];
		  pixels += 4;
		}
	      pixels += (w * 4 - rowstride);
	    }
	}
    }
  else
    {
      /* B&W cursor */

      int bpl;

      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmMask, 0, 0, NULL, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out1;

      w = bmi.bi.biWidth;
      h = ABS (bmi.bi.biHeight) / 2;
      
      bits = g_malloc0 (4 * w * h);
      
      /* masks */
      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmMask, 0, h*2, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out2;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);
      pixels = gdk_pixbuf_get_pixels (pixbuf);
      rowstride = gdk_pixbuf_get_rowstride (pixbuf);
      bpl = ((w-1)/32 + 1)*4;
#if 0
      for (y = 0; y < h*2; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      const gint bit = 7 - (x % 8);
	      printf ("%c ", ((bits[bpl*y+x/8])&(1<<bit)) ? ' ' : 'X');
	    }
	  printf ("\n");
	}
#endif

      for (y = 0; y < h; y++)
	{
	  const guchar *andp, *xorp;
	  if (bmi.bi.biHeight < 0)
	    {
	      andp = bits + bpl*y;
	      xorp = bits + bpl*(h+y);
	    }
	  else
	    {
	      andp = bits + bpl*(h-y-1);
	      xorp = bits + bpl*(h+h-y-1);
	    }
	  for (x = 0; x < w; x++)
	    {
	      const gint bit = 7 - (x % 8);
	      if ((*andp) & (1<<bit))
		{
		  if ((*xorp) & (1<<bit))
		    pixels[2] = pixels[1] = pixels[0] = 0xFF;
		  else
		    pixels[2] = pixels[1] = pixels[0] = 0;
		  pixels[3] = 0xFF;
		}
	      else
		{
		  pixels[2] = pixels[1] = pixels[0] = 0;
		  pixels[3] = 0;
		}
	      pixels += 4;
	      if (bit == 0)
		{
		  andp++;
		  xorp++;
		}
	    }
	  pixels += (w * 4 - rowstride);
	}
    }

  g_snprintf (buf, sizeof (buf), "%ld", ii.xHotspot);
  gdk_pixbuf_set_option (pixbuf, "x_hot", buf);

  g_snprintf (buf, sizeof (buf), "%ld", ii.yHotspot);
  gdk_pixbuf_set_option (pixbuf, "y_hot", buf);

  /* release temporary resources */
 out2:
  g_free (bits);
 out1:
  DeleteDC (hdc);
 out0:
  DeleteObject (ii.hbmColor);
  DeleteObject (ii.hbmMask);

  return pixbuf;
}

static GdkPixbuf *
_gdk_win32_cursor_get_image (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return gdk_win32_icon_to_pixbuf_libgtk_only (((GdkWin32Cursor *) cursor)->hcursor);
}

GdkCursor *
_gdk_win32_display_get_cursor_for_pixbuf (GdkDisplay *display, 
					  GdkPixbuf  *pixbuf,
					  gint        x,
					  gint        y)
{
  HCURSOR hcursor;

  g_return_val_if_fail (display == _gdk_display, NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  hcursor = _gdk_win32_pixbuf_to_hcursor (pixbuf, x, y);
  if (!hcursor)
    return NULL;
  return cursor_new_from_hcursor (hcursor, GDK_CURSOR_IS_PIXMAP);
}

gboolean
_gdk_win32_display_supports_cursor_alpha (GdkDisplay    *display)
{
  g_return_val_if_fail (display == _gdk_display, FALSE);

  return _gdk_win32_pixbuf_to_hicon_supports_alpha ();
}

gboolean 
_gdk_win32_display_supports_cursor_color (GdkDisplay    *display)
{
  g_return_val_if_fail (display == _gdk_display, FALSE);

  return TRUE;
}

void
_gdk_win32_display_get_default_cursor_size (GdkDisplay *display,
					    guint       *width,
					    guint       *height)
{
  g_return_if_fail (display == _gdk_display);

  if (width)
    *width = GetSystemMetrics (SM_CXCURSOR);
  if (height)
    *height = GetSystemMetrics (SM_CYCURSOR);
}

void     
_gdk_win32_display_get_maximal_cursor_size (GdkDisplay *display,
					    guint       *width,
					    guint       *height)
{
  g_return_if_fail (display == _gdk_display);
  
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
create_alpha_bitmap (gint     size,
		     guchar **outdata)
{
  BITMAPV5HEADER bi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bi, sizeof (BITMAPV5HEADER));
  bi.bV5Size = sizeof (BITMAPV5HEADER);
  bi.bV5Height = bi.bV5Width = size;
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
create_color_bitmap (gint     size,
		     guchar **outdata,
		     gint     bits)
{
  struct {
    BITMAPV4HEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } bmi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bmi, sizeof (bmi));
  bmi.bmiHeader.bV4Size = sizeof (BITMAPV4HEADER);
  bmi.bmiHeader.bV4Height = bmi.bmiHeader.bV4Width = size;
  bmi.bmiHeader.bV4Planes = 1;
  bmi.bmiHeader.bV4BitCount = bits;
  bmi.bmiHeader.bV4V4Compression = BI_RGB;

  /* when bits is 1, these will be used.
   * bmiColors[0] already zeroed from ZeroMemory()
   */
  bmi.bmiColors[1].rgbBlue = 0xFF;
  bmi.bmiColors[1].rgbGreen = 0xFF;
  bmi.bmiColors[1].rgbRed = 0xFF;

  hdc = GetDC (NULL);
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS,
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
  guchar *colordata, *colorrow, *maskdata, *maskbyte;
  gint width, height, size, i, i_offset, j, j_offset, rowstride;
  guint maskstride, mask_bit;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  /* The bitmaps are created square */
  size = MAX (width, height);

  hColorBitmap = create_alpha_bitmap (size, &colordata);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap = create_color_bitmap (size, &maskdata, 1);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* MSDN says mask rows are aligned to "LONG" boundaries */
  maskstride = (((size + 31) & ~31) >> 3);

  indata = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  if (width > height)
    {
      i_offset = 0;
      j_offset = (width - height) / 2;
    }
  else
    {
      i_offset = (height - width) / 2;
      j_offset = 0;
    }

  for (j = 0; j < height; j++)
    {
      colorrow = colordata + 4*(j+j_offset)*size + 4*i_offset;
      maskbyte = maskdata + (j+j_offset)*maskstride + i_offset/8;
      mask_bit = (0x80 >> (i_offset % 8));
      inrow = indata + (height-j-1)*rowstride;
      for (i = 0; i < width; i++)
	{
	  colorrow[4*i+0] = inrow[4*i+2];
	  colorrow[4*i+1] = inrow[4*i+1];
	  colorrow[4*i+2] = inrow[4*i+0];
	  colorrow[4*i+3] = inrow[4*i+3];
	  if (inrow[4*i+3] == 0)
	    maskbyte[0] |= mask_bit;	/* turn ON bit */
	  else
	    maskbyte[0] &= ~mask_bit;	/* turn OFF bit */
	  mask_bit >>= 1;
	  if (mask_bit == 0)
	    {
	      mask_bit = 0x80;
	      maskbyte++;
	    }
	}
    }

  *color = hColorBitmap;
  *mask = hMaskBitmap;

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
  guchar *colordata, *colorrow, *maskdata, *maskbyte;
  gint width, height, size, i, i_offset, j, j_offset, rowstride, nc, bmstride;
  gboolean has_alpha;
  guint maskstride, mask_bit;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  /* The bitmaps are created square */
  size = MAX (width, height);

  hColorBitmap = create_color_bitmap (size, &colordata, 24);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap = create_color_bitmap (size, &maskdata, 1);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* rows are always aligned on 4-byte boundarys */
  bmstride = size * 3;
  if (bmstride % 4 != 0)
    bmstride += 4 - (bmstride % 4);

  /* MSDN says mask rows are aligned to "LONG" boundaries */
  maskstride = (((size + 31) & ~31) >> 3);

  indata = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  nc = gdk_pixbuf_get_n_channels (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  if (width > height)
    {
      i_offset = 0;
      j_offset = (width - height) / 2;
    }
  else
    {
      i_offset = (height - width) / 2;
      j_offset = 0;
    }

  for (j = 0; j < height; j++)
    {
      colorrow = colordata + (j+j_offset)*bmstride + 3*i_offset;
      maskbyte = maskdata + (j+j_offset)*maskstride + i_offset/8;
      mask_bit = (0x80 >> (i_offset % 8));
      inrow = indata + (height-j-1)*rowstride;
      for (i = 0; i < width; i++)
	{
	  if (has_alpha && inrow[nc*i+3] < 128)
	    {
	      colorrow[3*i+0] = colorrow[3*i+1] = colorrow[3*i+2] = 0;
	      maskbyte[0] |= mask_bit;	/* turn ON bit */
	    }
	  else
	    {
	      colorrow[3*i+0] = inrow[nc*i+2];
	      colorrow[3*i+1] = inrow[nc*i+1];
	      colorrow[3*i+2] = inrow[nc*i+0];
	      maskbyte[0] &= ~mask_bit;	/* turn OFF bit */
	    }
	  mask_bit >>= 1;
	  if (mask_bit == 0)
	    {
	      mask_bit = 0x80;
	      maskbyte++;
	    }
	}
    }

  *color = hColorBitmap;
  *mask = hMaskBitmap;

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

HICON
gdk_win32_pixbuf_to_hicon_libgtk_only (GdkPixbuf *pixbuf)
{
  return _gdk_win32_pixbuf_to_hicon (pixbuf);
}

static void
gdk_win32_cursor_init (GdkWin32Cursor *cursor)
{
}
static void
gdk_win32_cursor_class_init(GdkWin32CursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (klass);

  object_class->finalize = _gdk_win32_cursor_finalize;
  
  cursor_class->get_image = _gdk_win32_cursor_get_image;
}
