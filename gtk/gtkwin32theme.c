/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Cosimo Cecchi <cosimoc@gnome.org>
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

#include <config.h>

#include "gtkwin32themeprivate.h"

#include "gtkwin32drawprivate.h"

#ifdef G_OS_WIN32

#include <windows.h>
#include <gdk/win32/gdkwin32.h>
#include <cairo-win32.h>

typedef HANDLE HTHEME;

#define UXTHEME_DLL "uxtheme.dll"

static HINSTANCE uxtheme_dll = NULL;
static gboolean use_xp_theme = FALSE;

typedef HRESULT (FAR PASCAL *GetThemeSysFontFunc)           (HTHEME hTheme, int iFontID, OUT LOGFONTW *plf);
typedef int (FAR PASCAL *GetThemeSysSizeFunc)               (HTHEME hTheme, int iSizeId);
typedef COLORREF (FAR PASCAL *GetThemeSysColorFunc)         (HTHEME hTheme,
							     int iColorID);
typedef HTHEME (FAR PASCAL *OpenThemeDataFunc)              (HWND hwnd,
							     LPCWSTR pszClassList);
typedef HRESULT (FAR PASCAL *CloseThemeDataFunc)            (HTHEME theme);
typedef HRESULT (FAR PASCAL *DrawThemeBackgroundFunc)       (HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
							     const RECT *pRect, const RECT *pClipRect);
typedef HRESULT (FAR PASCAL *EnableThemeDialogTextureFunc)  (HWND hwnd,
							     DWORD dwFlags);
typedef BOOL (FAR PASCAL *IsThemeActiveFunc)                (VOID);
typedef BOOL (FAR PASCAL *IsAppThemedFunc)                  (VOID);
typedef BOOL (FAR PASCAL *IsThemeBackgroundPartiallyTransparentFunc) (HTHEME hTheme,
								      int iPartId,
								      int iStateId);
typedef HRESULT (FAR PASCAL *DrawThemeParentBackgroundFunc) (HWND hwnd,
							     HDC hdc,
							     RECT *prc);
typedef HRESULT (FAR PASCAL *GetThemePartSizeFunc)          (HTHEME hTheme,
							     HDC hdc,
							     int iPartId,
							     int iStateId,
							     RECT *prc,
							     int eSize,
							     SIZE *psz);
typedef HRESULT (FAR PASCAL *GetThemeBackgroundExtentFunc)  (HTHEME hTheme,
							     HDC hdc,
							     int iPartId,
							     int iStateId,
                                                             const RECT *pContentRect,
                                                             RECT *pExtentRect);

static GetThemeSysFontFunc get_theme_sys_font = NULL;
static GetThemeSysColorFunc GetThemeSysColor = NULL;
static GetThemeSysSizeFunc GetThemeSysSize = NULL;
static OpenThemeDataFunc OpenThemeData = NULL;
static CloseThemeDataFunc CloseThemeData = NULL;
static DrawThemeBackgroundFunc draw_theme_background = NULL;
static EnableThemeDialogTextureFunc enable_theme_dialog_texture = NULL;
static IsThemeActiveFunc is_theme_active = NULL;
static IsAppThemedFunc is_app_themed = NULL;
static IsThemeBackgroundPartiallyTransparentFunc is_theme_partially_transparent = NULL;
static DrawThemeParentBackgroundFunc draw_theme_parent_background = NULL;
static GetThemePartSizeFunc GetThemePartSize = NULL;
static GetThemeBackgroundExtentFunc GetThemeBackgroundExtent = NULL;

#endif

static GHashTable *themes_by_class = NULL;

struct _GtkWin32Theme {
  char *class_name;
  gint ref_count;
#ifdef G_OS_WIN32
  HTHEME htheme;
#endif
};

GtkWin32Theme *
gtk_win32_theme_ref (GtkWin32Theme *theme)
{
  theme->ref_count++;

  return theme;
}

static gboolean
gtk_win32_theme_close (GtkWin32Theme *theme)
{
#ifdef G_OS_WIN32
  if (theme->htheme)
    {
      CloseThemeData (theme->htheme);
      theme->htheme = NULL;
      return TRUE;
    }
#endif
  return FALSE;
}

void
gtk_win32_theme_unref (GtkWin32Theme *theme)
{
  theme->ref_count--;

  if (theme->ref_count > 0)
    return;

  g_hash_table_remove (themes_by_class, theme->class_name);

  gtk_win32_theme_close (theme);
  g_free (theme->class_name);

  g_slice_free (GtkWin32Theme, theme);
}

gboolean
gtk_win32_theme_equal (GtkWin32Theme *theme1,
                       GtkWin32Theme *theme2)
{
  /* Themes are cached so they're guaranteed unique. */
  return theme1 == theme2;
}

#ifdef G_OS_WIN32

static GdkFilterReturn
invalidate_win32_themes (GdkXEvent *xevent,
                         GdkEvent  *event,
                         gpointer   unused)
{
  GHashTableIter iter;
  gboolean theme_was_open = FALSE;
  gpointer theme;
  MSG *msg;

  if (!GDK_IS_WIN32_WINDOW (event->any.window))
    return GDK_FILTER_CONTINUE;

  msg = (MSG *) xevent;
  if (msg->message != WM_THEMECHANGED)
    return GDK_FILTER_CONTINUE;

  g_hash_table_iter_init (&iter, themes_by_class);
  while (g_hash_table_iter_next (&iter, NULL, &theme))
    {
      theme_was_open |= gtk_win32_theme_close (theme);
    }
  if (theme_was_open)
    gtk_style_context_reset_widgets (gdk_display_get_default_screen (gdk_window_get_display (event->any.window)));

  return GDK_FILTER_CONTINUE;
}

static void
gtk_win32_theme_init (void)
{
  char *buf;
  char dummy;
  int n, k;

  if (uxtheme_dll)
    return;

  n = GetSystemDirectory (&dummy, 0);
  if (n <= 0)
    return;

  buf = g_malloc (n + 1 + strlen (UXTHEME_DLL));
  k = GetSystemDirectory (buf, n);
  if (k == 0 || k > n)
    {
      g_free (buf);
      return;
    }

  if (!G_IS_DIR_SEPARATOR (buf[strlen (buf) -1]))
    strcat (buf, G_DIR_SEPARATOR_S);
  strcat (buf, UXTHEME_DLL);

  uxtheme_dll = LoadLibrary (buf);
  g_free (buf);

  if (!uxtheme_dll)
    return;

  is_app_themed = (IsAppThemedFunc) GetProcAddress (uxtheme_dll, "IsAppThemed");
  if (is_app_themed)
    {
      is_theme_active = (IsThemeActiveFunc) GetProcAddress (uxtheme_dll, "IsThemeActive");
      OpenThemeData = (OpenThemeDataFunc) GetProcAddress (uxtheme_dll, "OpenThemeData");
      CloseThemeData = (CloseThemeDataFunc) GetProcAddress (uxtheme_dll, "CloseThemeData");
      draw_theme_background = (DrawThemeBackgroundFunc) GetProcAddress (uxtheme_dll, "DrawThemeBackground");
      enable_theme_dialog_texture = (EnableThemeDialogTextureFunc) GetProcAddress (uxtheme_dll, "EnableThemeDialogTexture");
      get_theme_sys_font = (GetThemeSysFontFunc) GetProcAddress (uxtheme_dll, "GetThemeSysFont");
      GetThemeSysColor = (GetThemeSysColorFunc) GetProcAddress (uxtheme_dll, "GetThemeSysColor");
      GetThemeSysSize = (GetThemeSysSizeFunc) GetProcAddress (uxtheme_dll, "GetThemeSysSize");
      is_theme_partially_transparent = (IsThemeBackgroundPartiallyTransparentFunc) GetProcAddress (uxtheme_dll, "IsThemeBackgroundPartiallyTransparent");
      draw_theme_parent_background = (DrawThemeParentBackgroundFunc) GetProcAddress (uxtheme_dll, "DrawThemeParentBackground");
      GetThemePartSize = (GetThemePartSizeFunc) GetProcAddress (uxtheme_dll, "GetThemePartSize");
      GetThemeBackgroundExtent = (GetThemeBackgroundExtentFunc) GetProcAddress (uxtheme_dll, "GetThemeBackgroundExtent");
    }

  if (is_app_themed && is_theme_active)
    {
      use_xp_theme = (is_app_themed () && is_theme_active ());
    }
  else
    {
      use_xp_theme = FALSE;
    }

  gdk_window_add_filter (NULL, invalidate_win32_themes, NULL);
}

static HTHEME
gtk_win32_theme_get_htheme (GtkWin32Theme *theme)
{
  guint16 *wclass;
  
  gtk_win32_theme_init ();

  if (theme->htheme)
    return theme->htheme;

  wclass = g_utf8_to_utf16 (theme->class_name, -1, NULL, NULL, NULL);
  theme->htheme  = OpenThemeData (NULL, wclass);
  g_free (wclass);

  return theme->htheme;
}

#endif /* G_OS_WIN32 */

static char *
canonicalize_class_name (const char *classname)
{
  /* Wine claims class names are case insensitive, so we convert them
     here to avoid multiple theme objects referencing the same HTHEME. */
  return g_ascii_strdown (classname, -1);
}

GtkWin32Theme *
gtk_win32_theme_lookup (const char *classname)
{
  GtkWin32Theme *theme;
  char *canonical_classname;

  if (G_UNLIKELY (themes_by_class == NULL))
    themes_by_class = g_hash_table_new (g_str_hash, g_str_equal);

  canonical_classname = canonicalize_class_name (classname);
  theme = g_hash_table_lookup (themes_by_class, canonical_classname);

  if (theme != NULL)
    {
      g_free (canonical_classname);
      return gtk_win32_theme_ref (theme);
    }

  theme = g_slice_new0 (GtkWin32Theme);
  theme->ref_count = 1;
  theme->class_name = canonical_classname;

  g_hash_table_insert (themes_by_class, theme->class_name, theme);

  return theme;
}

GtkWin32Theme *
gtk_win32_theme_parse (GtkCssParser *parser)
{
  GtkWin32Theme *theme;
  char *class_name;

  class_name = _gtk_css_parser_try_name (parser, TRUE);
  if (class_name == NULL)
    {
      _gtk_css_parser_error (parser, "Expected valid win32 theme name");
      return NULL;
    }

  theme = gtk_win32_theme_lookup (class_name);
  g_free (class_name);

  return theme;
}

cairo_surface_t *
gtk_win32_theme_create_surface (GtkWin32Theme *theme,
                                int            xp_part,
				int            state,
				int            margins[4],
				int            width,
                                int            height,
				int           *x_offs_out,
				int           *y_offs_out)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  int x_offs;
  int y_offs;
#ifdef G_OS_WIN32
  gboolean has_alpha;
  HDC hdc;
  RECT rect;
  SIZE size;
  HRESULT res;
  HTHEME htheme;
#endif

  x_offs = margins[3];
  y_offs = margins[0];

  width -= margins[3] + margins[1];
  height -= margins[0] + margins[2];

#ifdef G_OS_WIN32
  htheme = gtk_win32_theme_get_htheme (theme);
  if (htheme)
    {
      rect.left = 0;
      rect.top = 0;
      rect.right = width;
      rect.bottom = height;

      hdc = GetDC (NULL);
      res = GetThemePartSize (htheme, hdc, xp_part, state, &rect, 2 /*TS_DRAW*/, &size);
      ReleaseDC (NULL, hdc);

      if (res == S_OK)
        {
          x_offs += (width - size.cx) / 2;
          y_offs += (height - size.cy) / 2;
      
          width = size.cx;
          height = size.cy;

          rect.right = width;
          rect.bottom = height;
        }

      has_alpha = is_theme_partially_transparent (htheme, xp_part, state);
      if (has_alpha)
        surface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32, width, height);
      else
        surface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_RGB24, width, height);

      hdc = cairo_win32_surface_get_dc (surface);

      res = draw_theme_background (htheme, hdc, xp_part, state, &rect, &rect);

      *x_offs_out = x_offs;
      *y_offs_out = y_offs;

      if (res == S_OK)
        return surface;
    }
  else
#endif /* G_OS_WIN32 */
    {
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    }

  cr = cairo_create (surface);
  
  gtk_win32_draw_theme_background (cr, theme->class_name, xp_part, state, width, height);

  cairo_destroy (cr);
  
  *x_offs_out = x_offs;
  *y_offs_out = y_offs;

  return surface;
}

void
gtk_win32_theme_get_part_border (GtkWin32Theme  *theme,
                                 int             part,
                                 int             state,
                                 GtkBorder      *out_border)
{
#ifdef G_OS_WIN32
  HTHEME htheme = gtk_win32_theme_get_htheme (theme);
  RECT content, extent;
  HDC hdc;
  HRESULT res;

  if (use_xp_theme && GetThemeBackgroundExtent != NULL && htheme != NULL)
    {
      /* According to Wine source code, these values don't matter
       * because uxtheme.dll deals with margins internally. */
      content.left = content.top = 0;
      content.right = content.bottom = 100;

      hdc = GetDC (NULL);
      res = GetThemeBackgroundExtent (htheme, hdc, part, state, &content, &extent);
      ReleaseDC (NULL, hdc);

      if (SUCCEEDED (res))
        {
          out_border->top = content.top - extent.top;
          out_border->left = content.left - extent.left;
          out_border->bottom = extent.bottom - content.bottom;
          out_border->right = extent.right - content.right;
          return;
        }
    }
#endif

  gtk_win32_get_theme_margins (theme->class_name, part, state, out_border);
}

void
gtk_win32_theme_get_part_size (GtkWin32Theme  *theme,
                               int             part,
                               int             state,
                               int            *width,
                               int            *height)
{
#ifdef G_OS_WIN32
  HTHEME htheme = gtk_win32_theme_get_htheme (theme);
  SIZE size;
  HRESULT res;

  if (use_xp_theme && GetThemePartSize != NULL && htheme != NULL)
    {
      res = GetThemePartSize (htheme, NULL, part, state, NULL, 1 /* TS_TRUE */, &size);

      if (SUCCEEDED (res))
        {
          if (width)
            *width = size.cx;
          if (height)
            *height = size.cy;
          return;
        }
    }
#endif
  gtk_win32_get_theme_part_size (theme->class_name, part, state, width, height);
}

int
gtk_win32_theme_get_size (GtkWin32Theme *theme,
			  int            id)
{
#ifdef G_OS_WIN32
  if (use_xp_theme && GetThemeSysSize != NULL)
    {
      HTHEME htheme = gtk_win32_theme_get_htheme (theme);
      int size;

      /* If htheme is NULL it will just return the GetSystemMetrics value */
      size = GetThemeSysSize (htheme, id);
      /* fall through on invalid parameter */
      if (GetLastError () == 0)
        return size;
    }

  return GetSystemMetrics (id);
#else
  return gtk_win32_get_sys_metric (id);
#endif
}

void
gtk_win32_theme_get_color (GtkWin32Theme *theme,
                           gint           id,
                           GdkRGBA       *color)
{
#ifdef G_OS_WIN32
  HTHEME htheme;
  DWORD dcolor;

  if (use_xp_theme && GetThemeSysColor != NULL)
    {
      htheme = gtk_win32_theme_get_htheme (theme);

      /* if htheme is NULL, it will just return the GetSysColor() value */
      dcolor = GetThemeSysColor (htheme, id);
    }
  else
    dcolor = GetSysColor (id);

  color->alpha = 1.0;
  color->red = GetRValue (dcolor) / 255.0;
  color->green = GetGValue (dcolor) / 255.0;
  color->blue = GetBValue (dcolor) / 255.0;
#else
  gtk_win32_get_sys_color (id, color);
#endif
}

void
gtk_win32_theme_print (GtkWin32Theme *theme,
                       GString       *string)
{
  g_string_append (string, theme->class_name);
}
