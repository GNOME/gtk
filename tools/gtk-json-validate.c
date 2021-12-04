/* gtk-json-validate - Checks JSON data for errors
 * 
 * Copyright © 2013  Emmanuele Bassi
 *             2021  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi  <ebassi@gnome.org>
 *   Benjamin Otte    <otte@gnome.org>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <locale.h>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "gtk/json/gtkjsonparserprivate.h"

static char **files = NULL;

static GOptionEntry entries[] = {
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, NULL },
  { NULL },
};

static gboolean
validate (GFile *file)
{
  const GError *json_error;
  GError *error = NULL;
  GtkJsonParser *parser;
  GBytes *bytes;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (bytes == NULL)
    {
      /* Translators: the first %s is the program name, the second one
       * is the URI of the file, the third is the error message.
       */
      g_printerr (_("%s: %s: error opening file: %s\n"),
                  g_get_prgname (), g_file_get_uri (file), error->message);
      g_error_free (error);
      return FALSE;
    }

  parser = gtk_json_parser_new_for_bytes (bytes);

  while (gtk_json_parser_next (parser));

  json_error = gtk_json_parser_get_error (parser);
  if (json_error)
    {
      char *uri;
      const char *data, *start, *end;
      gsize start_line, start_bytes, start_offset, end_line, end_bytes, end_offset;
      GString *string;

      uri = g_file_get_uri (file);
      data = g_bytes_get_data (bytes, NULL);
      gtk_json_parser_get_error_offset (parser, &start_offset, &end_offset);
      start = data + start_offset;
      end = data + end_offset;
      gtk_json_parser_get_error_location (parser,
                                          &start_line, &start_bytes,
                                          &end_line, &end_bytes);
      
      string = g_string_new (NULL);
      g_string_append_printf (string, "%zu:%lu", 
                              start_line + 1,
                              g_utf8_pointer_to_offset (start - start_bytes, start) + 1);
      if (start_line != end_line || start_bytes != end_bytes)
        {
          g_string_append (string, "-");
          if (start_line != end_line)
            g_string_append_printf (string, "%zu:", end_line + 1);
          g_string_append_printf (string, "%lu", g_utf8_pointer_to_offset (end - end_bytes, end) + 1);
        }
      /* Translators: the first %s is the program name, the second one
       * is the URI of the file, the third is the file location and the
       * final one the error message.
       */
      g_printerr (_("%s: %s: error parsing file: %s: %s\n"),
                  g_get_prgname (), uri, string->str, json_error->message);
      g_string_free (string, TRUE);
      g_free (uri);
      gtk_json_parser_free (parser);
      g_bytes_unref (bytes);
      return FALSE;
    }

  gtk_json_parser_free (parser);
  g_bytes_unref (bytes);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  GOptionContext *context = NULL;
  GError *error = NULL;
  const char *description;
  const char *summary;
  gchar *param;
  gboolean res;
  int i;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  param = g_strdup_printf (("%s..."), _("FILE"));
  /* Translators: this message will appear after the usage string */
  /* and before the list of options.                              */
  summary = _("Validate JSON files.");
  description = _("json-glib-validate validates JSON data at the given URI.");

  context = g_option_context_new (param);
  g_option_context_set_summary (context, summary);
  g_option_context_set_description (context, description);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_parse (context, &argc, &argv, &error);
  g_option_context_free (context);

  g_free (param);

  if (error != NULL)
    {
      /* Translators: the %s is the program name. This error message
       * means the user is calling json-glib-validate without any
       * argument.
       */
      g_printerr (_("Error parsing commandline options: %s\n"), error->message);
      g_printerr ("\n");
      g_printerr (_("Try “%s --help” for more information."), g_get_prgname ());
      g_printerr ("\n");
      g_error_free (error);
      return 1;
    }

  if (files == NULL)
    {
      /* Translators: the %s is the program name. This error message
       * means the user is calling json-glib-validate without any
       * argument.
       */
      g_printerr (_("%s: missing files"), g_get_prgname ());
      g_printerr ("\n");
      g_printerr (_("Try “%s --help” for more information."), g_get_prgname ());
      g_printerr ("\n");
      return 1;
    }

  res = TRUE;
  i = 0;

  do
    {
      GFile *file = g_file_new_for_commandline_arg (files[i]);

      res = validate (file) && res;
      g_object_unref (file);
    }
  while (files[++i] != NULL);

  return res ? EXIT_SUCCESS : EXIT_FAILURE;
}
