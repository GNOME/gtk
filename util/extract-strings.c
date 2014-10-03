/* GTK - The GIMP Toolkit
 * Copyright (C) 2013 Red Hat, Inc.
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

#include <locale.h>
#include <glib.h>

typedef struct {
  GString *output;
  gboolean translatable;
  gchar *context;
  gchar *comments;
  GString *text;
} ParserData;

static void
start_element_handler (GMarkupParseContext  *contexts,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       gpointer              user_data,
                       GError              **error)
{
  ParserData *data = user_data;

  if (g_str_equal (element_name, "property") ||
      g_str_equal (element_name, "attribute") ||
      g_str_equal (element_name, "item"))
    {
      gboolean translatable;
      gchar *context;
      gchar *comments;

      g_markup_collect_attributes (element_name,
                                   attribute_names,
                                   attribute_values,
                                   error,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "name", NULL,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "value", NULL,
                                   G_MARKUP_COLLECT_TRISTATE, "translatable", &translatable,
                                   G_MARKUP_COLLECT_STRDUP|G_MARKUP_COLLECT_OPTIONAL, "context", &context,
                                   G_MARKUP_COLLECT_STRDUP|G_MARKUP_COLLECT_OPTIONAL, "comments", &comments,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "id", NULL,
                                   G_MARKUP_COLLECT_INVALID);

      if (translatable == TRUE)
        {
          data->translatable = TRUE;
          data->context = context;
          data->comments = comments;
          data->text = g_string_new ("");
        }
    }
}

static void
end_element_handler (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  ParserData *data = user_data;
  gchar **lines;
  gint i;

  if (!data->translatable)
    return;

  lines = g_strsplit (data->text->str, "\n", -1);

  if (data->comments)
    g_string_append_printf (data->output, "\n/* %s */\n",
                            data->comments);

  if (data->context)
    g_string_append_printf (data->output, "C_(\"%s\", ",
                            data->context);
  else
    g_string_append (data->output, "N_(");

  for (i = 0; lines[i]; i++)
    g_string_append_printf (data->output, "%s\"%s%s\"%s",
                            i > 0 ? "   " : "",
                            lines[i],
                            lines[i+1] ? "\\n" : "",
                            lines[i+1] ? "\n" : "");

  g_string_append (data->output, ");\n");

  g_strfreev (lines);

  g_free (data->comments);
  g_free (data->context);
  g_string_free (data->text, TRUE);

  data->comments = NULL;
  data->context = NULL;
  data->text = NULL;
  data->translatable = FALSE;
}

static void
text_handler (GMarkupParseContext  *context,
              const gchar          *text,
              gsize                 text_len,
              gpointer              user_data,
              GError              **error)
{
  ParserData *data = user_data;

  if (!data->translatable)
    return;

  g_string_append_len (data->text, text, text_len);
}

static const GMarkupParser parser = {
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL
};

int
main (int argc, char *argv[])
{
  gchar *contents;
  gsize length;
  GError *error;
  GMarkupParseContext *context;
  ParserData data;

  setlocale (LC_ALL, "");

  if (argc < 2)
    {
      g_printerr ("Expect a filename\n");
      return 1;
    }

  error = NULL;
  if (!g_file_get_contents (argv[1], &contents, &length, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      return 1;
    }

  data.output = g_string_new ("");
  data.translatable = FALSE;

  context = g_markup_parse_context_new (&parser, 0, &data, NULL);
  if (!g_markup_parse_context_parse (context, contents, length, &error))
    {
      g_markup_parse_context_free (context);
      g_free (contents);
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      return 1;
    }

  g_print ("%s", g_string_free (data.output, FALSE));

  return 0;
}
