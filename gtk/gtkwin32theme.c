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

#ifdef G_OS_WIN32

#include <cairo-win32.h>

#define UXTHEME_DLL "uxtheme.dll"

static HINSTANCE uxtheme_dll = NULL;
static gboolean use_xp_theme = FALSE;
static OSVERSIONINFO os_version;
static HTHEME needs_alpha_fixup1 = NULL;
static HTHEME needs_alpha_fixup2 = NULL;
static HTHEME needs_alpha_fixup3 = NULL;
static HTHEME needs_alpha_fixup4 = NULL;
static HTHEME needs_alpha_fixup5 = NULL;
static HTHEME needs_alpha_fixup6 = NULL;
static HTHEME needs_alpha_fixup7 = NULL;

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

static GetThemeSysFontFunc get_theme_sys_font = NULL;
static GetThemeSysColorFunc get_theme_sys_color = NULL;
static GetThemeSysSizeFunc get_theme_sys_metric = NULL;
static OpenThemeDataFunc open_theme_data = NULL;
static CloseThemeDataFunc close_theme_data = NULL;
static DrawThemeBackgroundFunc draw_theme_background = NULL;
static EnableThemeDialogTextureFunc enable_theme_dialog_texture = NULL;
static IsThemeActiveFunc is_theme_active = NULL;
static IsAppThemedFunc is_app_themed = NULL;
static IsThemeBackgroundPartiallyTransparentFunc is_theme_partially_transparent = NULL;
static DrawThemeParentBackgroundFunc draw_theme_parent_background = NULL;
static GetThemePartSizeFunc get_theme_part_size = NULL;

static GHashTable *hthemes_by_class = NULL;

static void
_gtk_win32_theme_init (void)
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
      open_theme_data = (OpenThemeDataFunc) GetProcAddress (uxtheme_dll, "OpenThemeData");
      close_theme_data = (CloseThemeDataFunc) GetProcAddress (uxtheme_dll, "CloseThemeData");
      draw_theme_background = (DrawThemeBackgroundFunc) GetProcAddress (uxtheme_dll, "DrawThemeBackground");
      enable_theme_dialog_texture = (EnableThemeDialogTextureFunc) GetProcAddress (uxtheme_dll, "EnableThemeDialogTexture");
      get_theme_sys_font = (GetThemeSysFontFunc) GetProcAddress (uxtheme_dll, "GetThemeSysFont");
      get_theme_sys_color = (GetThemeSysColorFunc) GetProcAddress (uxtheme_dll, "GetThemeSysColor");
      get_theme_sys_metric = (GetThemeSysSizeFunc) GetProcAddress (uxtheme_dll, "GetThemeSysSize");
      is_theme_partially_transparent = (IsThemeBackgroundPartiallyTransparentFunc) GetProcAddress (uxtheme_dll, "IsThemeBackgroundPartiallyTransparent");
      draw_theme_parent_background = (DrawThemeParentBackgroundFunc) GetProcAddress (uxtheme_dll, "DrawThemeParentBackground");
      get_theme_part_size = (GetThemePartSizeFunc) GetProcAddress (uxtheme_dll, "GetThemePartSize");
    }

  if (is_app_themed && is_theme_active)
    {
      use_xp_theme = (is_app_themed () && is_theme_active ());
    }
  else
    {
      use_xp_theme = FALSE;
    }

  hthemes_by_class = g_hash_table_new (g_str_hash, g_str_equal);

  memset (&os_version, 0, sizeof (os_version));
  os_version.dwOSVersionInfoSize = sizeof (os_version);
  GetVersionEx (&os_version);
  if (os_version.dwMajorVersion == 5)
    {
      needs_alpha_fixup1 = _gtk_win32_lookup_htheme_by_classname ("scrollbar");
      needs_alpha_fixup2 = _gtk_win32_lookup_htheme_by_classname ("toolbar");
      needs_alpha_fixup3 = _gtk_win32_lookup_htheme_by_classname ("button");
      needs_alpha_fixup4 = _gtk_win32_lookup_htheme_by_classname ("header");
      needs_alpha_fixup5 = _gtk_win32_lookup_htheme_by_classname ("trackbar");
      needs_alpha_fixup6 = _gtk_win32_lookup_htheme_by_classname ("status");
      needs_alpha_fixup7 = _gtk_win32_lookup_htheme_by_classname ("rebar");
    }
}

HTHEME
_gtk_win32_lookup_htheme_by_classname (const char *class)
{
  HTHEME theme;
  guint16 *wclass;
  char *lower;
  
  _gtk_win32_theme_init ();

  lower = g_ascii_strdown (class, -1);

  theme = (HTHEME)  g_hash_table_lookup (hthemes_by_class, lower);
  if (theme)
    {
      g_free (lower);
      return theme;
    }

  wclass = g_utf8_to_utf16 (lower, -1, NULL, NULL, NULL);
  theme  = open_theme_data (NULL, wclass);
  g_free (wclass);

  if (theme == NULL)
    {
      g_free (lower);
      return NULL;
    }

  /* Takes ownership of lower: */
  g_hash_table_insert (hthemes_by_class, lower, theme);

  return theme;
}

#else

HTHEME
_gtk_win32_lookup_htheme_by_classname (const char *class)
{
  return NULL;
}

#endif /* G_OS_WIN32 */

cairo_surface_t *
_gtk_win32_theme_part_create_surface (HTHEME theme,
                                      int    xp_part,
				      int    state,
				      int    margins[4],
				      int    width,
                                      int    height,
				      int   *x_offs_out,
				      int   *y_offs_out)
{
  cairo_surface_t *surface;
  GdkRGBA color;
  cairo_t *cr;
  int x_offs;
  int y_offs;
#ifdef G_OS_WIN32
  gboolean has_alpha;
  HDC hdc;
  RECT rect;
  SIZE size;
  HRESULT res;
#endif

  x_offs = margins[3];
  y_offs = margins[0];

  width -= margins[3] + margins[1];
  height -= margins[0] + margins[2];

#ifdef G_OS_WIN32
  rect.left = 0;
  rect.top = 0;
  rect.right = width;
  rect.bottom = height;

  hdc = GetDC (NULL);
  res = get_theme_part_size (theme, hdc, xp_part, state, &rect, 2, &size);
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

  has_alpha = is_theme_partially_transparent (theme, xp_part, state);
  if (has_alpha)
    surface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32, width, height);
  else
    surface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_RGB24, width, height);

  hdc = cairo_win32_surface_get_dc (surface);

  res = draw_theme_background (theme, hdc, xp_part, state, &rect, &rect);

  /* XP Can't handle rendering some parts on an alpha target */
  if (has_alpha && 
      (theme == needs_alpha_fixup1 ||
       theme == needs_alpha_fixup2 ||
       (theme == needs_alpha_fixup3 && xp_part == 4) ||
       theme == needs_alpha_fixup4 ||
       theme == needs_alpha_fixup5 ||
       theme == needs_alpha_fixup6 ||
       theme == needs_alpha_fixup7))
    {
      cairo_surface_t *img = cairo_win32_surface_get_image (surface);
      guint32 *data = (guint32 *)cairo_image_surface_get_data (img);
      int i, j;
      GdiFlush ();

      for (i = 0; i < width; i++)
	{
	  for (j = 0; j < height; j++)
	    {
	      if (data[i+j*width] != 0)
		data[i+j*width] |= 0xff000000;
	    }
	}
    }

  *x_offs_out = x_offs;
  *y_offs_out = y_offs;

  if (res == S_OK)
    return surface;

#else /* !G_OS_WIN32 */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
#endif /* G_OS_WIN32 */

  cr = cairo_create (surface);
  
  /* XXX: Do something better here (like printing the theme parts) */
  gdk_rgba_parse (&color, "pink");
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_paint (cr);

  cairo_destroy (cr);
  
  *x_offs_out = x_offs;
  *y_offs_out = y_offs;

  return surface;
}

int
_gtk_win32_theme_int_parse (GtkCssParser      *parser,
			    int               *value)
{
  char *class;
  int arg;

  if (_gtk_css_parser_try (parser,
			   "-gtk-win32-size",
			   TRUE))
    {
      if (!_gtk_css_parser_try (parser, "(", TRUE))
	{
	  _gtk_css_parser_error (parser,
				 "Expected '(' after '-gtk-win32-size'");
	  return 0;
	}

      class = _gtk_css_parser_try_name (parser, TRUE);
      if (class == NULL)
	{
	  _gtk_css_parser_error (parser,
				 "Expected name as first argument to  '-gtk-win32-size'");
	  return 0;
	}

      if (! _gtk_css_parser_try (parser, ",", TRUE))
	{
	  g_free (class);
	  _gtk_css_parser_error (parser,
				 "Expected ','");
	  return 0;
	}

      if (!_gtk_css_parser_try_int (parser, &arg))
	{
	  g_free (class);
	  _gtk_css_parser_error (parser, "Expected a valid integer value");
	  return 0;
	}

      if (!_gtk_css_parser_try (parser, ")", TRUE))
	{
	  _gtk_css_parser_error (parser,
				 "Expected ')'");
	  return 0;
	}

#ifdef G_OS_WIN32
      if (use_xp_theme && get_theme_sys_metric != NULL)
	{
	  HTHEME theme = _gtk_win32_lookup_htheme_by_classname (class);

	  /* If theme is NULL it will just return the GetSystemMetrics value */
	  *value = get_theme_sys_metric (theme, arg);
	}
      else
	*value = GetSystemMetrics (arg);
#else
      *value = 1;
#endif

      g_free (class);

      return 1;
    }

  return -1;
}

gboolean
_gtk_win32_theme_color_resolve (const char *theme_class,
				gint id,
				GdkRGBA *color)
{
#ifdef G_OS_WIN32
  DWORD dcolor;

  if (use_xp_theme && get_theme_sys_color != NULL)
    {
      HTHEME theme = _gtk_win32_lookup_htheme_by_classname (theme_class);

      /* if theme is NULL, it will just return the GetSystemColor()
         value */
      dcolor = get_theme_sys_color (theme, id);
    }
  else
    dcolor = GetSysColor (id);

  color->alpha = 1.0;
  color->red = GetRValue (dcolor) / 255.0;
  color->green = GetGValue (dcolor) / 255.0;
  color->blue = GetBValue (dcolor) / 255.0;
#else
  gdk_rgba_parse (color, "pink");
#endif
  return TRUE;
}

const char *
_gtk_win32_theme_get_default (void)
{
#ifdef G_OS_WIN32
  _gtk_win32_theme_init ();
  
  if (use_xp_theme)
    return (os_version.dwMajorVersion >= 6) ? "gtk-win32" : "gtk-win32-xp";
#endif
  return "gtk-win32-classic";
}

