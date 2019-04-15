/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Chun-wei Fan <fanc999@yahoo.com.tw>
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

#ifdef HAVE_HARFBUZZ

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

#include "gtkpangofontutilsprivate.h"

#ifdef G_OS_WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <hb.h>
#endif

#ifdef HAVE_PANGOFT
# include <pango/pangofc-font.h>

/* On Windows, we need to check whether the PangoFont is a FontConfig Font or a Win32/GDI font,
 * and acquire/release the FT_Face accordingly.
 */
# ifdef G_OS_WIN32
#  define FT_EXT_ITEM_INIT(f)   \
          PANGO_IS_FC_FONT(f) ? \
          NULL : \
          g_new0 (gtk_win32_ft_items, 1)

#  define PANGO_FONT_GET_FT_FACE(f,m,i) \
          PANGO_IS_FC_FONT(f) ?          \
          pango_fc_font_lock_face (PANGO_FC_FONT (f)) : \
          pangowin32_font_get_ftface (f, m, i)

#  define PANGO_FONT_RELEASE_FT_FACE(f,i) \
          PANGO_IS_FC_FONT(f) ?           \
          pango_fc_font_unlock_face (PANGO_FC_FONT (f)) : \
          pangowin32_font_release_ftface (i);

#  define RELEASE_EXTRA_FT_ITEMS(i) pangowin32_font_release_extra_ft_items(i)

# else /* On non-Windows, acquire/release the FT_Face as we did before */
#  define FT_EXT_ITEM_INIT(f) NULL
#  define PANGO_FONT_GET_FT_FACE(f,m,i) pango_fc_font_lock_face (PANGO_FC_FONT (f))
#  define PANGO_FONT_RELEASE_FT_FACE(f,i) pango_fc_font_unlock_face (PANGO_FC_FONT (f))
#  define RELEASE_EXTRA_FT_ITEMS(i) FALSE
# endif
#elif defined (G_OS_WIN32) /* HAVE_PANGOFT */

/* Until Pango uses HarfBuzz for shaping on all platforms, we need to go through FreeType */

# define FT_EXT_ITEM_INIT(f) g_new0 (gtk_win32_ft_items, 1)
# define PANGO_FONT_GET_FT_FACE(f,m,i) pangowin32_font_get_ftface (f, m, i)
# define PANGO_FONT_RELEASE_FT_FACE(f,i) pangowin32_font_release_ftface (i)
# define RELEASE_EXTRA_FT_ITEMS(i) pangowin32_font_release_extra_ft_items(i)

#endif /* G_OS_WIN32 */

/* Note: The following is for both builds with and without PangoFT support on Windows */
#ifdef G_OS_WIN32
#include <pango/pangowin32.h>

/* Check for TTC fonts and get their font data */
# define FONT_TABLE_TTCF  (('t' << 0) + ('t' << 8) + ('c' << 16) + ('f' << 24))

typedef struct _gtk_win32_ft_items
{
  FT_Library ft_lib;
  FT_Byte *font_data_stream;
  FT_Face face;
  HDC hdc;
  LOGFONTW *logfont;
  HFONT hfont;
  PangoWin32FontCache *cache;
} gtk_win32_ft_items;

#define WIN32_FT_FACE_FAIL(msg) \
{ \
  g_warning(msg); \
  goto ft_face_fail; \
}

static FT_Face
pangowin32_font_get_ftface (PangoFont          *font,
                            PangoFontMap       *font_map,
                            gtk_win32_ft_items *item)
{
  PangoWin32FontCache *cache = NULL;
  LOGFONTW *logfont = NULL;
  HFONT hfont = NULL;
  HFONT hfont_orig = NULL;
  HDC hdc = NULL;
  DWORD is_ttc_font;
  FT_Face face;
  FT_Byte *font_stream = NULL;
  unsigned char buf[4];
  gsize font_size;

  if (item == NULL)
    item = g_new0 (gtk_win32_ft_items, 1);

  cache = pango_win32_font_map_get_font_cache (font_map);

  if (!cache)
    WIN32_FT_FACE_FAIL ("Failed to acquire PangoWin32FontCache");

  logfont = pango_win32_font_logfontw (font);

  if (logfont == NULL)
    WIN32_FT_FACE_FAIL ("Unable to acquire LOGFONT from PangoFont");

  hfont = pango_win32_font_cache_loadw (cache, logfont);

  if (hfont == NULL)
    WIN32_FT_FACE_FAIL ("Unable to acquire HFONT from PangoWin32FontCache with LOGFONT (LOGFONT invalid?)");

  hdc = GetDC (NULL);

  if (hdc == NULL)
    WIN32_FT_FACE_FAIL ("Failed to acquire DC");

  if (item->ft_lib == NULL)
    {
      if (FT_Init_FreeType (&item->ft_lib) != FT_Err_Ok)
        WIN32_FT_FACE_FAIL ("Failed to initialize FreeType for PangoWin32Font->FT_Face transformation");
    }

  hfont_orig = SelectObject (hdc, hfont);
  if (hfont_orig == HGDI_ERROR)
    WIN32_FT_FACE_FAIL ("SelectObject() for the PangoFont failed");

  /* is_ttc_font is GDI_ERROR if the HFONT does not refer to a font in a TTC when,
   * specifying FONT_TABLE_TTCF for the Font Table type, otherwise it is 1,
   * so try again without specifying FONT_TABLE_TTCF if is_ttc_font is not 1
   */
  is_ttc_font = GetFontData (hdc, FONT_TABLE_TTCF, 0, &buf, 1);

  if (is_ttc_font == 1)
    font_size = GetFontData (hdc, FONT_TABLE_TTCF, 0, NULL, 0);
  else
    font_size = GetFontData (hdc, 0, 0, NULL, 0);

  if (font_size == GDI_ERROR)
    WIN32_FT_FACE_FAIL ("Could not acquire font size from GetFontData()");

  /* Now, get the font data stream that we need for creating the FT_Face */
  font_stream = g_malloc (font_size);
  if (GetFontData (hdc,
                   is_ttc_font == 1 ? FONT_TABLE_TTCF : 0,
                   0,
                   font_stream,
                   font_size) == GDI_ERROR)
    {
      WIN32_FT_FACE_FAIL ("Unable to get data stream of font!");
    }

  /* Finally, create the FT_Face we need */
  if (FT_New_Memory_Face (item->ft_lib,
                          font_stream,
                          font_size,
                          0,
                          &face) == FT_Err_Ok)
    {
      /* We need to track these because we can only release/free those items *after* we
       * are done with them in FreeType
       */
      item->cache = cache;
      item->logfont = logfont;
      item->hdc = hdc;
      item->hfont = hfont;
      item->font_data_stream = font_stream;
      item->face = face;
      return item->face;
    }

  else
    WIN32_FT_FACE_FAIL ("Unable to create FT_Face from font data stream!");

ft_face_fail:
  if (font_stream != NULL)
    g_free (font_stream);

  if (hdc != NULL)
    ReleaseDC (NULL, hdc);

  if (cache != NULL && hfont != NULL)
    pango_win32_font_cache_unload (cache, hfont);

  if (logfont != NULL)
    g_free (logfont);

  return NULL;
}

#undef WIN32_FT_FACE_FAIL

static void
pangowin32_font_release_ftface (gtk_win32_ft_items *item)
{
  FT_Done_Face (item->face);
  g_free (item->font_data_stream);
  ReleaseDC (NULL, item->hdc);
  item->hdc = NULL;
  pango_win32_font_cache_unload (item->cache, item->hfont);
  g_free (item->logfont);
}

gboolean
pangowin32_font_release_extra_ft_items (gpointer ft_item)
{
  gtk_win32_ft_items *item = NULL;

  if (ft_item == NULL)
    return FALSE;

  item = (gtk_win32_ft_items *)ft_item;
  if (item->ft_lib != NULL)
    {
      FT_Done_Library (item->ft_lib);
      item->ft_lib = NULL;
    }

  return TRUE;

}

/* if we are using native Windows (PangoWin32),
 * use native Windows API for language/script names
 */
static BOOL CALLBACK
get_win32_all_locales_scripts (LPWSTR locale_w, DWORD flags, LPARAM param)
{
  wchar_t *langname_w = NULL;
  wchar_t *locale_abbrev_w = NULL;
  gchar *langname, *locale_abbrev, *locale, *p;
  gint i;
  hb_language_t lang;
  GHashTable *ht_scripts_langs = (GHashTable *)param;

  gint langname_size, locale_abbrev_size;
  langname_size = GetLocaleInfoEx (locale_w, LOCALE_SLOCALIZEDDISPLAYNAME, langname_w, 0);
  if (langname_size == 0)
    return FALSE;

  langname_w = g_new0 (wchar_t, langname_size);

  if (langname_size == 0)
    return FALSE;

  GetLocaleInfoEx (locale_w, LOCALE_SLOCALIZEDDISPLAYNAME, langname_w, langname_size);
  langname = g_utf16_to_utf8 (langname_w, -1, NULL, NULL, NULL);
  locale = g_utf16_to_utf8 (locale_w, -1, NULL, NULL, NULL);
  p = strchr (locale, '-');
  lang = hb_language_from_string (locale, p ? p - locale : -1);
  if (g_hash_table_lookup (ht_scripts_langs, lang) == NULL)
    g_hash_table_insert (ht_scripts_langs, lang, langname);

  /* track 3-letter ISO639-2/3 language codes as well */
  locale_abbrev_size = GetLocaleInfoEx (locale_w, LOCALE_SABBREVLANGNAME, locale_abbrev_w, 0);
  if (locale_abbrev_size > 0)
    {
      locale_abbrev_w = g_new0 (wchar_t, locale_abbrev_size);
      GetLocaleInfoEx (locale_w, LOCALE_SABBREVLANGNAME, locale_abbrev_w, locale_abbrev_size);

	  locale_abbrev = g_utf16_to_utf8 (locale_abbrev_w, -1, NULL, NULL, NULL);
      lang = hb_language_from_string (locale_abbrev, -1);
      if (g_hash_table_lookup (ht_scripts_langs, lang) == NULL)
        g_hash_table_insert (ht_scripts_langs, lang, langname);

      g_free (locale_abbrev);
      g_free (locale_abbrev_w);
    }

  g_free (locale);
  g_free (langname_w);

  return TRUE;
}

GHashTable *
_gtk_font_chooser_widget_get_win32_locales (void)
{
  static GHashTable *ht_langtag_locales = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited))
    {
      gchar *locales, *script;
      ht_langtag_locales = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
      EnumSystemLocalesEx (&get_win32_all_locales_scripts, LOCALE_ALL, (LPARAM)ht_langtag_locales, NULL);
      g_once_init_leave (&inited, 1);
    }

  return ht_langtag_locales;
}

#endif /* G_OS_WIN32 */

gpointer
_gtk_pango_font_init_extra_ft_items (PangoFont *font)
{
  return FT_EXT_ITEM_INIT (font);
}

FT_Face
_gtk_pango_font_get_ft_face (PangoFont    *font,
                             PangoFontMap *font_map,
                             gpointer      ft_items)
{
  return PANGO_FONT_GET_FT_FACE (font, font_map, ft_items);
}

void
_gtk_pango_font_release_ft_face (PangoFont *font,
                                 gpointer   ft_items)
{
  PANGO_FONT_RELEASE_FT_FACE (font, ft_items);
}

gboolean
_gtk_pango_font_release_ft_items (gpointer ft_items)
{
  return RELEASE_EXTRA_FT_ITEMS (ft_items);
}
#endif /* HAVE_HARFBUZZ */
