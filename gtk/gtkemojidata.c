/* gtkemoji-data.c
 *
 * Copyright 2017 Red Hat, Inc.
 * Copyright 2026 Christian Hergert
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkemojidataprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"

static GBytes *
get_emoji_data_by_language (const char *lang)
{
  GBytes *bytes;
  char *path;
  GError *error = NULL;

  path = g_strconcat ("/org/gtk/libgtk/emoji/", lang, ".data", NULL);
  bytes = g_resources_lookup_data (path, 0, &error);

  if (bytes)
    {
      g_debug ("Found emoji data for %s in resource %s", lang, path);
      g_free (path);
      return bytes;
    }

  if (g_error_matches (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND))
    {
      char *filename;
      char *gresource_name;
      GMappedFile *file;

      g_clear_error (&error);

      gresource_name = g_strconcat (lang, ".gresource", NULL);
      filename = g_build_filename (_gtk_get_data_prefix (),
                                   "share", "gtk-4.0", "emoji",
                                   gresource_name, NULL);
      g_clear_pointer (&gresource_name, g_free);
      file = g_mapped_file_new (filename, FALSE, NULL);

      if (file)
        {
          GBytes *data;
          GResource *resource;

          data = g_mapped_file_get_bytes (file);
          g_mapped_file_unref (file);

          resource = g_resource_new_from_data (data, NULL);
          g_bytes_unref (data);

          g_debug ("Registering resource for Emoji data for %s from file %s",
                   lang, filename);
          g_resources_register (resource);
          g_resource_unref (resource);

          bytes = g_resources_lookup_data (path, 0, NULL);
          if (bytes)
            {
              g_debug ("Found emoji data for %s in resource %s", lang, path);
              g_free (path);
              g_free (filename);
              return bytes;
            }
        }

      g_free (filename);
    }

  g_clear_error (&error);
  g_free (path);

  return NULL;
}

GBytes *
gtk_emoji_data_load (void)
{
  GBytes *bytes;
  const char *lang;

  lang = pango_language_to_string (gtk_get_default_language ());
  bytes = get_emoji_data_by_language (lang);
  if (bytes)
    return bytes;

  if (strchr (lang, '-'))
    {
      char q[5];
      int i;

      for (i = 0; lang[i] != '-' && i < 4; i++)
        q[i] = lang[i];
      q[i] = '\0';

      bytes = get_emoji_data_by_language (q);
      if (bytes)
        return bytes;
    }

  bytes = get_emoji_data_by_language ("en");
  g_assert (bytes);

  return bytes;
}

char *
gtk_emoji_data_dup_text (GVariant *record,
                         gunichar  modifier)
{
  GVariant *codes;
  GString *text;
  gsize i;

  g_return_val_if_fail (record != NULL, NULL);

  codes = g_variant_get_child_value (record, 0);
  text = g_string_sized_new (4 * g_variant_n_children (codes) + 1);

  for (i = 0; i < g_variant_n_children (codes); i++)
    {
      gunichar code;
      char utf8[6];
      int n_bytes;

      g_variant_get_child (codes, i, "u", &code);
      if (code == 0)
        code = modifier != 0 ? modifier : 0xfe0f;
      else if (code == 0x1f3fb)
        code = modifier;

      if (code != 0)
        {
          n_bytes = g_unichar_to_utf8 (code, utf8);
          g_string_append_len (text, utf8, n_bytes);
        }
    }

  g_variant_unref (codes);

  return g_string_free (text, FALSE);
}

gboolean
gtk_emoji_data_has_variations (GVariant *emoji_data)
{
  GVariant *codes;
  gsize i;
  gboolean has_variations;

  has_variations = FALSE;
  codes = g_variant_get_child_value (emoji_data, 0);
  for (i = 0; i < g_variant_n_children (codes); i++)
    {
      gunichar code;

      g_variant_get_child (codes, i, "u", &code);
      if (code == 0 || code == 0x1f3fb)
        {
          has_variations = TRUE;
          break;
        }
    }
  g_variant_unref (codes);

  return has_variations;
}

static gboolean
match_tokens (const char **term_tokens,
              const char **hit_tokens)
{
  int i;
  int j;
  gboolean matched;

  matched = TRUE;

  for (i = 0; term_tokens[i]; i++)
    {
      for (j = 0; hit_tokens[j]; j++)
        if (g_str_has_prefix (hit_tokens[j], term_tokens[i]))
          goto one_matched;

      matched = FALSE;
      break;

one_matched:
      continue;
    }

  return matched;
}

gboolean
gtk_emoji_data_matches (GVariant    *record,
                        const char **term_tokens)
{
  const char *name_en;
  const char *name;
  const char **keywords_en;
  const char **keywords;
  char **name_tokens_en;
  char **name_tokens;
  gboolean matched;

  g_variant_get_child (record, 1, "&s", &name_en);
  g_variant_get_child (record, 2, "&s", &name);
  g_variant_get_child (record, 3, "^a&s", &keywords_en);
  g_variant_get_child (record, 4, "^a&s", &keywords);

  name_tokens_en = g_str_tokenize_and_fold (name_en, "en", NULL);
  name_tokens = g_str_tokenize_and_fold (name, "en", NULL);

  matched = match_tokens ((const char **) term_tokens, (const char **) name_tokens_en) ||
            match_tokens ((const char **) term_tokens, (const char **) name_tokens) ||
            match_tokens ((const char **) term_tokens, keywords_en) ||
            match_tokens ((const char **) term_tokens, keywords);

  g_free (keywords_en);
  g_free (keywords);
  g_strfreev (name_tokens_en);
  g_strfreev (name_tokens);

  return matched;
}
