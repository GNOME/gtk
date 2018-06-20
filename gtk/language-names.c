#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>
#include <errno.h>
#include <locale.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#if defined (HAVE_HARFBUZZ) && defined (HAVE_PANGOFT)
#include <hb-ot.h>

#include "language-names.h"

#define ISO_CODES_DATADIR ISO_CODES_PREFIX "/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX "/share/locale"

static GHashTable *language_map;

static char *
get_first_item_in_semicolon_list (const char *list)
{
  char **items;
  char  *item;

  items = g_strsplit (list, "; ", 2);

  item = g_strdup (items[0]);
  g_strfreev (items);

  return item;
}

static char *
capitalize_utf8_string (const char *str)
{
  char first[8] = { 0 };

  if (!str)
    return NULL;

  g_unichar_to_utf8 (g_unichar_totitle (g_utf8_get_char (str)), first);

  return g_strconcat (first, g_utf8_offset_to_pointer (str, 1), NULL);
}

static char *
get_display_name (const char *language)
{
  const char  *translated;
  char *tmp;
  char *name;

  translated = dgettext ("iso_639", language);

  tmp = get_first_item_in_semicolon_list (translated);
  name = capitalize_utf8_string (tmp);
  g_free (tmp);

  return name;
}

static void
languages_parse_start_tag (GMarkupParseContext  *ctx,
                           const char           *element_name,
                           const char          **attr_names,
                           const char          **attr_values,
                           gpointer              user_data,
                           GError              **error)
{
  const char *ccode_longB;
  const char *ccode_longT;
  const char *ccode;
  const char *ccode_id;
  const char *lang_name;
  char *display_name;

  if (!(g_str_equal (element_name, "iso_639_entry") ||
        g_str_equal (element_name, "iso_639_3_entry")) ||
        attr_names == NULL ||
        attr_values == NULL)
    return;

  ccode = NULL;
  ccode_longB = NULL;
  ccode_longT = NULL;
  ccode_id = NULL;
  lang_name = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "iso_639_1_code"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 2)
                return;
              ccode = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "iso_639_2B_code"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                return;
              ccode_longB = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "iso_639_2T_code"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                return;
              ccode_longT = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "id"))
        {
          if (**attr_values)
            {
              if (strlen (*attr_values) != 2 &&
                  strlen (*attr_values) != 3)
                return;
              ccode_id = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "name"))
        {
          lang_name = *attr_values;
        }

      ++attr_names;
      ++attr_values;
    }

  if (lang_name == NULL)
    return;

  display_name = get_display_name (lang_name);

  if (ccode != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode),
                         g_strdup (display_name));

  if (ccode_longB != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode_longB),
                         g_strdup (display_name));

  if (ccode_longT != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode_longT),
                         g_strdup (display_name));

  if (ccode_id != NULL)
    g_hash_table_insert (language_map,
                         pango_language_from_string (ccode_id),
                         g_strdup (display_name));

  g_free (display_name);
}

static void
languages_variant_init (const char *variant)
{
  gboolean res;
  gsize    buf_len;
  char *buf = NULL;
  char *filename = NULL;
  GError *error = NULL;

  bindtextdomain (variant, ISO_CODES_LOCALESDIR);
  bind_textdomain_codeset (variant, "UTF-8");

  error = NULL;
  filename = g_strconcat (ISO_CODES_DATADIR, "/", variant, ".xml", NULL);
  res = g_file_get_contents (filename, &buf, &buf_len, &error);
  if (res)
    {
      GMarkupParseContext *ctx = NULL;
      GMarkupParser parser = { languages_parse_start_tag, NULL, NULL, NULL, NULL };

      ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

      g_free (error);
      error = NULL;
      res = g_markup_parse_context_parse (ctx, buf, buf_len, &error);
      g_free (ctx);

      if (!res)
        g_warning ("Failed to parse '%s': %s\n", filename, error->message);
    }
  else
    g_warning ("Failed to load '%s': %s\n", filename, error->message);

  g_free (filename);
  g_free (buf);
}

static void
languages_init (void)
{
  if (language_map)
    return;

  language_map = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  languages_variant_init ("iso_639");
  languages_variant_init ("iso_639_3");
}

const char *
get_language_name (PangoLanguage *language)
{
  languages_init ();

  return (const char *) g_hash_table_lookup (language_map, language);
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
