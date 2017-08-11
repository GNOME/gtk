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

#include <json-glib/json-glib.h>
#include <string.h>

gboolean
parse_code (GVariantBuilder *b,
            const char      *code)
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
      g_variant_builder_add (b, "u", u);
    }

  return TRUE;
}

static const char *blacklist[] = {
  "child",
  "adult",
  "older adult",
  "woman with headscarf",
  "bearded person",
  "breast-feeding",
  "mage",
  "fairy",
  "vampire",
  "merperson",
  "merman",
  "mermaid",
  " elf", // avoid matching selfie
  "genie",
  "zombie",
  "in steamy room",
  "climbing",
  "in lotus position",
  "person in bed",
  "man in suit levitating",
  "horse racing",
  "snowboarder",
  "golfing",
  "love-you gesture",
  "palms up together",
  NULL
};

int
main (int argc, char *argv[])
{
  JsonParser *parser;
  JsonNode *root;
  JsonArray *array;
  GError *error = NULL;
  guint length, i;
  GVariantBuilder builder;
  GVariant *v;
  GString *s;
  GBytes *bytes;

  if (argc != 3)
    {
      g_print ("Usage: emoji-convert INPUT OUTPUT\n");
      return 1;
    }

  parser = json_parser_new ();

  if (!json_parser_load_from_file (parser, argv[1], &error))
    {
      g_error ("%s", error->message);
      return 1;
    }

  root = json_parser_get_root (parser);
  array = json_node_get_array (root);
  length = json_array_get_length (array);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ausaau)"));
  i = 0;
  while (i < length)
    {
      JsonNode *node = json_array_get_element (array, i);
      JsonObject *obj = json_node_get_object (node);
      GVariantBuilder b1, b2;
      const char *name;
      const char *code;
      int j;
      gboolean skip;

      i++;

      g_variant_builder_init (&b1, G_VARIANT_TYPE ("au"));
      g_variant_builder_init (&b2, G_VARIANT_TYPE ("aau"));

      name = json_object_get_string_member (obj, "name");
      code = json_object_get_string_member (obj, "code");

      if (strcmp (name, "world map") == 0)
        continue;

      skip = FALSE;
      for (j = 0; blacklist[j]; j++)
        {
          if (strstr (name, blacklist[j]) != 0)
            skip = TRUE;
        }
      if (skip)
        continue;

      if (!parse_code (&b1, code))
        return 1;

      while (i < length)
        {
          JsonNode *node2 = json_array_get_element (array, i);
          JsonObject *obj2 = json_node_get_object (node2);
          const char *name2;
          const char *code2;
          GVariantBuilder b22;

          name2 = json_object_get_string_member (obj2, "name");
          code2 = json_object_get_string_member (obj2, "code");

          if (!strstr (name2, "skin tone") || !g_str_has_prefix (name2, name))
            break;

          g_variant_builder_init (&b22, G_VARIANT_TYPE ("au"));
          if (!parse_code (&b22, code2))
            return 1;

          g_variant_builder_add (&b2, "au", &b22);
          i++;
        }

      g_variant_builder_add (&builder, "(ausaau)", &b1, name, &b2);
    }

  v = g_variant_builder_end (&builder);
  bytes = g_variant_get_data_as_bytes (v);
  if (!g_file_set_contents (argv[2], g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), &error))
    {
      g_error ("%s", error->message);
      return 1;
    }

  return 0;
}
