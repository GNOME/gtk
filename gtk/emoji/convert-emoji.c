/* convert-emoji.c: A tool to create an Emoji data GVariant
 * Copyright 2017, Red Hat, Inc.
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

/* Build with gcc -o convert-emoji convert-emoji.c `pkg-config --cflags --libs json-glib-1.0`
 */

/* The format of the generated data is: a(auss).
 * Each member of the array has the following fields:
 * au - sequence of unicode codepoints. If the
 *      sequence contains a 0, it marks the point
 *      where skin tone modifiers should be inserted
 * s  - name, e.g. "man worker"
 * s  - shortname, for completion. This includes
 *      colons to mark the ends, e.g. ":guardsman:"
 * as - keywords, e.g. "man", "worker"
 */
#include <json-glib/json-glib.h>
#include <string.h>

gboolean
parse_code (GVariantBuilder *b,
            const char      *code,
            GString         *name_key)
{
  g_auto(GStrv) strv = NULL;
  int j;

  strv = g_strsplit (code, " ", -1);
  for (j = 0; strv[j]; j++)
    {
      guint32 u;
      char *end;

      u = (guint32) g_ascii_strtoull (strv[j], &end, 16);
      if (*end != '\0')
        {
          g_error ("failed to parse code: %s\n", strv[j]);
          return FALSE;
        }
      if (0x1f3fb <= u && u <= 0x1f3ff)
        g_variant_builder_add (b, "u", 0);
      else
        {
          g_variant_builder_add (b, "u", u);
          if (j > 0)
            g_string_append_c (name_key, '-');
          g_string_append_printf (name_key, "%x", u);
        }
    }

  return TRUE;
}

int
main (int argc, char *argv[])
{
  JsonParser *parser;
  JsonNode *root;
  JsonArray *array;
  JsonObject *ro;
  JsonNode *node;
  const char *name;
  JsonObjectIter iter;
  GError *error = NULL;
  guint length, i;
  GVariantBuilder builder;
  GVariant *v;
  GString *s;
  GHashTable *names;
  GString *name_key;

  if (argc != 4)
    {
      g_print ("Usage: emoji-convert INPUT INPUT1 OUTPUT\n");
      return 1;
    }

  parser = json_parser_new ();

  if (!json_parser_load_from_file (parser, argv[2], &error))
    {
      g_error ("%s", error->message);
      return 1;
    }

  root = json_parser_get_root (parser);
  ro = json_node_get_object (root);
  json_object_iter_init (&iter, ro);

  names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)json_object_unref);
  name_key = g_string_new ("");

  while (json_object_iter_next (&iter, &name, &node))
    {
      JsonObject *obj = json_node_get_object (node);
      const char *unicode;

      unicode = json_object_get_string_member (obj, "unicode");
      g_hash_table_insert (names, g_strdup (unicode), json_object_ref (obj));
    }

  g_object_unref (parser);

  parser = json_parser_new ();

  if (!json_parser_load_from_file (parser, argv[1], &error))
    {
      g_error ("%s", error->message);
      return 1;
    }

  root = json_parser_get_root (parser);
  array = json_node_get_array (root);
  length = json_array_get_length (array);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(aussas)"));
  i = 0;
  while (i < length)
    {
      JsonNode *node = json_array_get_element (array, i);
      JsonObject *obj = json_node_get_object (node);
      GVariantBuilder b1;
      GVariantBuilder b2;
      const char *name;
      const char *shortname;
      char *code;
      int j, k;
      gboolean skip;
      gboolean has_variations;
      JsonObject *obj2;
      JsonArray *kw;
      char **name_tokens;

      i++;

      g_variant_builder_init (&b1, G_VARIANT_TYPE ("au"));

      name = json_object_get_string_member (obj, "name");
      code = g_strdup (json_object_get_string_member (obj, "code"));

      has_variations = FALSE;
      while (i < length)
        {
          JsonNode *node2 = json_array_get_element (array, i);
          JsonObject *obj2 = json_node_get_object (node2);
          const char *name2;
          const char *code2;

          name2 = json_object_get_string_member (obj2, "name");
          code2 = json_object_get_string_member (obj2, "code");

          if (!strstr (name2, "skin tone") || !g_str_has_prefix (name2, name))
            break;

          if (!has_variations)
            {
              has_variations = TRUE;
              g_free (code);
              code = g_strdup (code2);
            }
          i++;
        }

      g_string_set_size (name_key, 0);
      if (!parse_code (&b1, code, name_key))
        return 1;

      g_variant_builder_init (&b2, G_VARIANT_TYPE ("as"));
      name_tokens = g_str_tokenize_and_fold (name, "en", NULL);
      for (j = 0; j < g_strv_length (name_tokens); j++)
        g_variant_builder_add (&b2, "s", name_tokens[j]);

      obj2 = g_hash_table_lookup (names, name_key->str);
      if (obj2)
        {
          shortname = json_object_get_string_member (obj2, "shortname");
          kw = json_object_get_array_member (obj2, "keywords");
          for (k = 0; k < json_array_get_length (kw); k++)
            {
              char **folded;
              char **ascii;

              folded = g_str_tokenize_and_fold (json_array_get_string_element (kw, k), "en", &ascii);
              for (j = 0; j < g_strv_length (folded); j++)
                {
                  if (!g_strv_contains ((const char * const *)name_tokens, folded[j]))
                    g_variant_builder_add (&b2, "s", folded[j]);
                }
              for (j = 0; j < g_strv_length (ascii); j++)
                {
                  if (!g_strv_contains ((const char * const *)name_tokens, ascii[j]))
                    g_variant_builder_add (&b2, "s", ascii[j]);
                }
              g_strfreev (folded);
              g_strfreev (ascii);
            }
        }
      else
        shortname = "";

      g_strfreev (name_tokens);

      g_variant_builder_add (&builder, "(aussas)", &b1, name, shortname, &b2);
    }

  v = g_variant_builder_end (&builder);
  if (g_str_has_suffix (argv[3], ".json"))
    {
      JsonNode *node;
      char *out;

      node = json_gvariant_serialize (v);
      out = json_to_string (node, TRUE);
      if (!g_file_set_contents (argv[3], out, -1, &error))
        {
          g_error ("%s", error->message);
          return 1;
        }
    }
  else
    {
      GBytes *bytes;

      bytes = g_variant_get_data_as_bytes (v);
      if (!g_file_set_contents (argv[3], g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), &error))
        {
          g_error ("%s", error->message);
          return 1;
        }
    }

  return 0;
}
