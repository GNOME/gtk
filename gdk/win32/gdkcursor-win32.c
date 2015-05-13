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
#include "gdkwin32.h"

#include "gdkdisplay-win32.h"

#ifdef __MINGW32__
#include <w32api.h>
#endif

#include "xcursors.h"

typedef struct _DefaultCursor {
  char *name;
  char *id;
} DefaultCursor;

static DefaultCursor default_cursors[] = {
  { "appstarting", IDC_APPSTARTING },
  { "arrow", IDC_ARROW },
  { "cross", IDC_CROSS },
  { "hand",  IDC_HAND },
  { "help",  IDC_HELP },
  { "ibeam", IDC_IBEAM },
  /* an X cursor name, for compatibility with GTK: */
  { "left_ptr_watch", IDC_APPSTARTING },
  { "sizeall", IDC_SIZEALL },
  { "sizenesw", IDC_SIZENESW },
  { "sizens", IDC_SIZENS },
  { "sizenwse", IDC_SIZENWSE },
  { "sizewe", IDC_SIZEWE },
  { "uparrow", IDC_UPARROW },
  { "wait", IDC_WAIT },
  /* css cursor names: */
  { "default", IDC_ARROW },
  { "pointer", IDC_HAND },
  { "progress", IDC_APPSTARTING },
  { "crosshair", IDC_CROSS },
  { "text", IDC_IBEAM },
  { "move", IDC_SIZEALL },
  { "not-allowed", IDC_NO },
  { "ew-resize", IDC_SIZEWE },
  { "ns-resize", IDC_SIZENS },
  { "nesw-resize", IDC_SIZENESW },
  { "nwse-resize", IDC_SIZENWSE }
};

static HCURSOR
hcursor_from_x_cursor (gint          i,
                       GdkCursorType cursor_type)
{
  gint j, x, y, ofs;
  HCURSOR rv;
  gint w, h;
  guchar *and_plane, *xor_plane;

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

static HCURSOR
win32_cursor_create_hcursor (Win32Cursor *cursor)
{
  HCURSOR result;

  switch (cursor->load_type)
    {
      case GDK_WIN32_CURSOR_LOAD_FROM_FILE:
        result = LoadImageW (NULL,
                             cursor->resource_name,
                             IMAGE_CURSOR,
                             cursor->width,
                             cursor->height,
                             cursor->load_flags);
        break;
      case GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_NULL:
        result = LoadImageA (NULL,
                             (const gchar *) cursor->resource_name,
                             IMAGE_CURSOR,
                             cursor->width,
                             cursor->height,
                             cursor->load_flags);
        break;
      case GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_THIS:
        result = LoadImageA (_gdk_app_hmodule,
                             (const gchar *) cursor->resource_name,
                             IMAGE_CURSOR,
                             cursor->width,
                             cursor->height,
                             cursor->load_flags);
        break;
      case GDK_WIN32_CURSOR_CREATE:
        result = hcursor_from_x_cursor (cursor->xcursor_number,
                                        cursor->cursor_type);
        break;
      default:
        result = NULL;
    }

  return result;
}

static Win32Cursor *
win32_cursor_new (GdkWin32CursorLoadType load_type,
                  gpointer               resource_name,
                  gint                   width,
                  gint                   height,
                  guint                  load_flags,
                  gint                   xcursor_number,
                  GdkCursorType          cursor_type)
{
  Win32Cursor *result;

  result = g_new (Win32Cursor, 1);
  result->load_type = load_type;
  result->resource_name = resource_name;
  result->width = width;
  result->height = height;
  result->load_flags = load_flags;
  result->xcursor_number = xcursor_number;
  result->cursor_type = cursor_type;

  return result;
}


static void
win32_cursor_destroy (gpointer data)
{
  Win32Cursor *cursor = data;

  /* resource_name could be a resource ID (uint16_t stored as a pointer),
   * which shouldn't be freed.
   */
  if (cursor->load_type == GDK_WIN32_CURSOR_LOAD_FROM_FILE)
    g_free (cursor->resource_name);

  g_free (cursor);
}

static void
win32_cursor_theme_load_from (Win32CursorTheme *theme,
                              gint              size,
                              const gchar      *dir)
{
  GDir *gdir;
  const gchar *filename;
  HCURSOR hcursor;

  gdir = g_dir_open (dir, 0, NULL);

  if (gdir == NULL)
    return;

  while ((filename = g_dir_read_name (gdir)) != NULL)
    {
      gchar *fullname;
      gunichar2 *filenamew;
      gchar *cursor_name;
      gchar *dot;
      Win32Cursor *cursor;

      fullname = g_strconcat (dir, "/", filename, NULL);
      filenamew = g_utf8_to_utf16 (fullname, -1, NULL, NULL, NULL);
      g_free (fullname);

      if (filenamew == NULL)
        continue;

      hcursor = LoadImageW (NULL, filenamew, IMAGE_CURSOR, size, size,
                            LR_LOADFROMFILE | (size == 0 ? LR_DEFAULTSIZE : 0));

      if (hcursor == NULL)
        {
          g_free (filenamew);
          continue;
        }

      DestroyCursor (hcursor);
      dot = strchr (filename, '.');

      cursor_name = dot ? g_strndup (filename, dot - filename) : g_strdup (filename);

      cursor = win32_cursor_new (GDK_WIN32_CURSOR_LOAD_FROM_FILE,
                                 filenamew,
                                 size,
                                 size,
                                 LR_LOADFROMFILE | (size == 0 ? LR_DEFAULTSIZE : 0),
                                 0,
                                 0);
      g_hash_table_insert (theme->named_cursors, cursor_name, cursor);
    }
}

static void
win32_cursor_theme_load_from_dirs (Win32CursorTheme *theme,
                                   const gchar      *name,
                                   gint              size)
{
  gchar *theme_dir;
  const gchar * const *dirs;
  gint i;

  dirs = g_get_system_data_dirs ();

  /* <prefix>/share/icons */
  for (i = 0; dirs[i]; i++)
    {
      theme_dir = g_strconcat (dirs[i], "/icons/", name, "/cursors", NULL);
      win32_cursor_theme_load_from (theme, size, theme_dir);
      g_free (theme_dir);
    }

  /* ~/.icons */
  theme_dir = g_strconcat (g_get_home_dir (), "/icons/", name, "/cursors", NULL);
  win32_cursor_theme_load_from (theme, size, theme_dir);
  g_free (theme_dir);
}

static void
win32_cursor_theme_load_system (Win32CursorTheme *theme,
                                gint              size)
{
  gint i;
  HCURSOR hcursor;
  Win32Cursor *cursor;

  for (i = 0; i < G_N_ELEMENTS (cursors); i++)
    {

      if (cursors[i].name == NULL)
        break;

      hcursor = NULL;

      /* Prefer W32 cursors */
      if (cursors[i].builtin)
        hcursor = LoadImageA (NULL, cursors[i].builtin, IMAGE_CURSOR,
                              size, size,
                              LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0));

      /* Fall back to X cursors, but only if we've got no theme cursor */
      if (hcursor == NULL && g_hash_table_lookup (theme->named_cursors, cursors[i].name) == NULL)
        hcursor = hcursor_from_x_cursor (i, cursors[i].type);

      if (hcursor == NULL)
        continue;

      DestroyCursor (hcursor);
      cursor = win32_cursor_new (GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_NULL,
                                 (gpointer) cursors[i].builtin,
                                 size,
                                 size,
                                 LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0),
                                 0,
                                 0);
      g_hash_table_insert (theme->named_cursors,
                           g_strdup (cursors[i].name),
                           cursor);
    }

  for (i = 0; i < G_N_ELEMENTS (default_cursors); i++)
    {
      if (default_cursors[i].name == NULL)
        break;

      hcursor = LoadImageA (NULL, default_cursors[i].id, IMAGE_CURSOR, size, size,
                            LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0));

      if (hcursor == NULL)
        continue;

      DestroyCursor (hcursor);
      cursor = win32_cursor_new (GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_NULL,
                                 (gpointer) default_cursors[i].id,
                                 size,
                                 size,
                                 LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0),
                                 0,
                                 0);
      g_hash_table_insert (theme->named_cursors,
                           g_strdup (default_cursors[i].name),
                           cursor);
    }
}

Win32CursorTheme *
win32_cursor_theme_load (const gchar *name,
                         gint         size)
{
  Win32CursorTheme *result = g_new0 (Win32CursorTheme, 1);

  result->named_cursors = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 win32_cursor_destroy);

  if (strcmp (name, "system") == 0)
    {
      win32_cursor_theme_load_from_dirs (result, "Adwaita", size);
      win32_cursor_theme_load_system (result, size);
    }
  else
    {
      win32_cursor_theme_load_from_dirs (result, name, size);
    }

  if (g_hash_table_size (result->named_cursors) > 0)
    return result;

  win32_cursor_theme_destroy (result);
  return NULL;
}

void
win32_cursor_theme_destroy (Win32CursorTheme *theme)
{
  g_hash_table_destroy (theme->named_cursors);
  g_free (theme);
}

Win32Cursor *
win32_cursor_theme_get_cursor (Win32CursorTheme *theme,
                               const gchar      *name)
{
  return g_hash_table_lookup (theme->named_cursors, name);
}

static HCURSOR
hcursor_from_type (GdkCursorType cursor_type)
{
  gint i = 0;

  if (cursor_type != GDK_BLANK_CURSOR)
    {
      for (i = 0; i < G_N_ELEMENTS (cursors); i++)
	if (cursors[i].type == cursor_type)
	  break;

      if (i >= G_N_ELEMENTS (cursors) || !cursors[i].name)
	return NULL;

      /* Use real Win32 cursor if possible */
      if (cursors[i].builtin)
        return LoadImageA (NULL, cursors[i].builtin, IMAGE_CURSOR, 0, 0,
                           LR_SHARED | LR_DEFAULTSIZE);
    }

  return hcursor_from_x_cursor (i, cursor_type);
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
    g_warning (G_STRLOC ": DestroyCursor (%p) failed: %lu", private->hcursor, GetLastError ());

  g_free (private->name);

  G_OBJECT_CLASS (gdk_win32_cursor_parent_class)->finalize (object);
}

static HCURSOR
hcursor_idc_from_name (const gchar *name)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (default_cursors); i++)
    {
      if (strcmp (default_cursors[i].name, name) != 0)
        continue;

      return LoadImageA (NULL, default_cursors[i].id, IMAGE_CURSOR, 0, 0,
                         LR_SHARED | LR_DEFAULTSIZE);
    }

  return NULL;
}

static HCURSOR
hcursor_x_from_name (const gchar *name)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (cursors); i++)
    if (cursors[i].name == NULL || strcmp (cursors[i].name, name) == 0)
      return hcursor_from_x_cursor (i, cursors[i].type);

  return NULL;
}

static HCURSOR
hcursor_from_theme (GdkDisplay  *display,
                    const gchar *name)
{
  Win32CursorTheme *theme;
  Win32Cursor *theme_cursor;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  if (name == NULL)
    return NULL;

  theme = _gdk_win32_display_get_cursor_theme (win32_display);
  theme_cursor = win32_cursor_theme_get_cursor (theme, name);

  if (theme_cursor == NULL)
    return NULL;

  return win32_cursor_create_hcursor (theme_cursor);
}

static HCURSOR
hcursor_from_name (GdkDisplay  *display,
                   const gchar *name)
{
  HCURSOR hcursor;

  if (strcmp (name, "none") == 0)
    return hcursor_from_type (GDK_BLANK_CURSOR);

  /* Try current theme first */
  hcursor = hcursor_from_theme (display, name);

  if (hcursor != NULL)
    return hcursor;

  hcursor = hcursor_idc_from_name (name);

  if (hcursor != NULL)
    return hcursor;

  hcursor = hcursor_x_from_name (name);

  return hcursor;
}

static GdkCursor*
cursor_new_from_hcursor (HCURSOR        hcursor,
			 const gchar   *name,
			 GdkCursorType  cursor_type)
{
  GdkWin32Cursor *private;
  GdkCursor *cursor;

  private = g_object_new (GDK_TYPE_WIN32_CURSOR,
                          "cursor-type", cursor_type,
                          "display", _gdk_display,
			  NULL);

  private->name = g_strdup (name);

  private->hcursor = hcursor;
  cursor = (GdkCursor*) private;

  return cursor;
}

static gboolean
_gdk_win32_cursor_update (GdkWin32Display *win32_display,
                          GdkWin32Cursor  *cursor)
{
  HCURSOR hcursor = NULL;
  Win32CursorTheme *theme;
  Win32Cursor *theme_cursor;

  /* Do nothing if this is not a named cursor. */
  if (cursor->name == NULL)
    return FALSE;

  theme = _gdk_win32_display_get_cursor_theme (win32_display);
  theme_cursor = win32_cursor_theme_get_cursor (theme, cursor->name);

  if (theme_cursor != NULL)
    hcursor = win32_cursor_create_hcursor (theme_cursor);

  if (hcursor == NULL)
    {
      g_warning (G_STRLOC ": Unable to load %s from the cursor theme", cursor->name);

      hcursor = hcursor_idc_from_name (cursor->name);

      if (hcursor == NULL)
        hcursor = hcursor_x_from_name (cursor->name);

      if (hcursor == NULL)
        return FALSE;
    }

  if (GetCursor () == cursor->hcursor)
    SetCursor (hcursor);

  if (!DestroyCursor (cursor->hcursor))
    g_warning (G_STRLOC ": DestroyCursor (%p) failed: %lu", cursor->hcursor, GetLastError ());

  cursor->hcursor = hcursor;

  return TRUE;
}

void
_gdk_win32_display_update_cursors (GdkWin32Display *display)
{
  GHashTableIter iter;
  const char *name;
  GdkWin32Cursor *cursor;

  g_hash_table_iter_init (&iter, display->cursor_cache);

  while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer *) &cursor))
    _gdk_win32_cursor_update (display, cursor);
}

void
_gdk_win32_display_init_cursors (GdkWin32Display *display)
{
  display->cursor_cache = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 NULL,
                                                 g_object_unref);
  display->cursor_theme_name = g_strdup ("system");
}

void
_gdk_win32_display_finalize_cursors (GdkWin32Display *display)
{
  g_free (display->cursor_theme_name);

  if (display->cursor_theme)
    win32_cursor_theme_destroy (display->cursor_theme);

  g_hash_table_destroy (display->cursor_cache);
}


GdkCursor*
_gdk_win32_display_get_cursor_for_type (GdkDisplay   *display,
					GdkCursorType cursor_type)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  gchar *cursor_name;
  HCURSOR hcursor;
  GdkCursor *result;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  enum_class = g_type_class_ref (GDK_TYPE_CURSOR_TYPE);
  enum_value = g_enum_get_value (enum_class, cursor_type);
  cursor_name = g_strdup (enum_value->value_nick);
  g_strdelimit (cursor_name, "-", '_');
  g_type_class_unref (enum_class);

  result = g_hash_table_lookup (win32_display->cursor_cache, cursor_name);
  if (result)
    {
      g_free (cursor_name);
      return g_object_ref (result);
    }

  hcursor = hcursor_from_name (display, cursor_name);

  if (hcursor == NULL)
    hcursor = hcursor_from_type (cursor_type);

  if (hcursor == NULL)
    g_warning ("gdk_cursor_new_for_display: no cursor %d found", cursor_type);
  else
    GDK_NOTE (CURSOR, g_print ("gdk_cursor_new_for_display: %d: %p\n",
			       cursor_type, hcursor));

  result = cursor_new_from_hcursor (hcursor, cursor_name, cursor_type);

  if (result == NULL)
    return result;

  /* Blank cursor case */
  if (cursor_type == GDK_BLANK_CURSOR ||
      !cursor_name ||
      g_str_equal (cursor_name, "none") ||
      g_str_equal (cursor_name, "blank_cursor"))
    {
      g_free (cursor_name);
      return result;
    }

  g_hash_table_insert (win32_display->cursor_cache,
                       cursor_name,
                       g_object_ref (result));

  return result;
}

GdkCursor*
_gdk_win32_display_get_cursor_for_name (GdkDisplay  *display,
				        const gchar *name)
{
  HCURSOR hcursor = NULL;
  GdkCursor *result;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  result = g_hash_table_lookup (win32_display->cursor_cache, name);
  if (result)
    return g_object_ref (result);

  hcursor = hcursor_from_name (display, name);

  /* allow to load named cursor resources linked into the executable */
  if (!hcursor)
    hcursor = LoadCursor (_gdk_app_hmodule, name);

  if (hcursor == NULL)
    return NULL;

  result = cursor_new_from_hcursor (hcursor, name, GDK_X_CURSOR);

  /* Blank cursor case */
  if (!name ||
      g_str_equal (name, "none") ||
      g_str_equal (name, "blank_cursor"))
    return result;

  g_hash_table_insert (win32_display->cursor_cache,
                       g_strdup (name),
                       g_object_ref (result));

  return result;
}

GdkPixbuf *
gdk_win32_icon_to_pixbuf_libgtk_only (HICON hicon,
                                      gdouble *x_hot,
                                      gdouble *y_hot)
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

  if (x_hot)
    *x_hot = ii.xHotspot;
  if (y_hot)
    *y_hot = ii.yHotspot;

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

static cairo_surface_t *
_gdk_win32_cursor_get_surface (GdkCursor *cursor,
			       gdouble *x_hot,
			       gdouble *y_hot)
{
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface;

  g_return_val_if_fail (cursor != NULL, NULL);

  pixbuf = gdk_win32_icon_to_pixbuf_libgtk_only (((GdkWin32Cursor *) cursor)->hcursor, x_hot, y_hot);

  if (pixbuf == NULL)
    return NULL;

  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);
  g_object_unref (pixbuf);

  return surface;
}

GdkCursor *
_gdk_win32_display_get_cursor_for_surface (GdkDisplay      *display,
					   cairo_surface_t *surface,
					   gdouble          x,
					   gdouble          y)
{
  HCURSOR hcursor;
  GdkPixbuf *pixbuf;
  gint width, height;

  g_return_val_if_fail (display == _gdk_display, NULL);
  g_return_val_if_fail (surface != NULL, NULL);

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);
  pixbuf = gdk_pixbuf_get_from_surface (surface,
                                        0,
                                        0,
                                        width,
                                        height);

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  hcursor = _gdk_win32_pixbuf_to_hcursor (pixbuf, x, y);

  g_object_unref (pixbuf);
  if (!hcursor)
    return NULL;
  return cursor_new_from_hcursor (hcursor, NULL, GDK_CURSOR_IS_PIXMAP);
}

gboolean
_gdk_win32_display_supports_cursor_alpha (GdkDisplay    *display)
{
  g_return_val_if_fail (display == _gdk_display, FALSE);

  return TRUE;
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

  /* TODO: Use per-monitor DPI functions (8.1 and newer) or
   * calculate DPI ourselves and use that, assuming that 72 dpi
   * corresponds to 32x32 cursors. Take into account that DPI
   * can be artificially increased by the user to make stuff bigger.
   */

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

  if (gdk_pixbuf_get_has_alpha (pixbuf))
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

  cursor_class->get_surface = _gdk_win32_cursor_get_surface;
}
