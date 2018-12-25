#include "config.h"

#include <glib.h>
#include <pango/pango.h>

#if defined (G_OS_WIN32) && defined (HAVE_HARFBUZZ)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <hb-ot.h>

#include "language-names.h"

/* if we are using native Windows (PangoWin32), use DirectWrite */
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

static GHashTable *
languages_init (void)
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

const char *
get_language_name (PangoLanguage *language)
{
  GHashTable *ht_lang = languages_init ();

  return (const char *) g_hash_table_lookup (ht_lang, language);
}

const char *
get_language_name_for_tag (guint32 tag)
{
  hb_language_t lang;
  const char *s;

  lang = hb_ot_tag_to_language (tag);
  s = hb_language_to_string (lang);

  return get_language_name (pango_language_from_string (s));
}

#endif