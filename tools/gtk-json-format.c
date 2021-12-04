/* gtk-json-format - Formats JSON data
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

#ifdef G_OS_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gio/gunixoutputstream.h>
#endif
#include <fcntl.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <stdlib.h>
#include <locale.h>

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "gtk/json/gtkjsonparserprivate.h"
#include "gtk/json/gtkjsonprinterprivate.h"

#if defined (G_OS_WIN32) && !defined (HAVE_UNISTD_H)
#include <io.h>

#define STDOUT_FILENO 1
#endif

static char **files = NULL;
static char *output = NULL;
static gboolean ascii = FALSE;
static gboolean prettify = FALSE;
static int indent_spaces = 2;

static GOptionEntry entries[] = {
  { "prettify", 'p', 0, G_OPTION_ARG_NONE, &prettify, N_("Prettify output"), NULL },
  { "indent-spaces", 'i', 0, G_OPTION_ARG_INT, &indent_spaces, N_("Indentation spaces"), N_("SPACES") },
  { "ascii", 0, 0, G_OPTION_ARG_NONE, &ascii, N_("Convert to ASCII instead of UTF-8"), NULL },
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, N_("Output file"), N_("FILE") },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, N_("FILE…") },
  { NULL },
};

typedef struct
{
  gchar buffer[4096];
  gsize size;
  int fd;
  gboolean close;
  int error;
} Writer;

static void
writer_init (Writer   *writer,
             int       fd,
             gboolean  close)
{
  writer->size = 0;
  writer->fd = fd;
  writer->close = close;
  writer->error = 0;
}

static void
writer_flush (Writer     *writer,
              const char *data,
              gsize       len)
{
  if (writer->error)
    return;

again:
  if (write (writer->fd, data, len) < 0)
    {
      if (errno == EINTR)
        goto again;
      writer->error = errno;
    }
}

static int
writer_finish (Writer *writer)
{
  writer_flush (writer, writer->buffer, writer->size);

  if (writer->close)
    g_close (writer->fd, NULL);

  return writer->error;
}

static void
writer_write (GtkJsonPrinter *printer,
              const char     *s,
              gpointer        data)
{
  Writer *writer = data;
  int len = strlen (s);

  if (sizeof (writer->buffer) - writer->size < len)
    {
      writer_flush (writer, writer->buffer, writer->size);
      writer->size = 0;
    }

  if (sizeof (writer->buffer) <= len)
    {
      writer_flush (writer, s, len);
      return;
    }

  memcpy (writer->buffer + writer->size, s, len);
  writer->size += len;
}

static void
parse_and_print (GtkJsonParser  *parser,
                 GtkJsonPrinter *printer)
{
  while (TRUE)
    {
      char *name = gtk_json_parser_get_member_name (parser);

      switch (gtk_json_parser_get_node (parser))
        {
        case GTK_JSON_NONE:
          if (gtk_json_printer_get_depth (printer) == 0)
            return;
          gtk_json_printer_end (printer);
          gtk_json_parser_end (parser);
          break;

        case GTK_JSON_NULL:
          gtk_json_printer_add_null (printer, name);
          break;

        case GTK_JSON_BOOLEAN:
          gtk_json_printer_add_boolean (printer,
                                        name,
                                        gtk_json_parser_get_boolean (parser));
          break;

        case GTK_JSON_NUMBER:
          gtk_json_printer_add_number (printer,
                                       name,
                                       gtk_json_parser_get_number (parser));
          break;

        case GTK_JSON_STRING:
          { 
            char *s = gtk_json_parser_get_string (parser);
            gtk_json_printer_add_string (printer, name, s);
            g_free (s);
          }
          break;

        case GTK_JSON_OBJECT:
          gtk_json_printer_start_object (printer, name);
          gtk_json_parser_start_object (parser);
          continue;

        case GTK_JSON_ARRAY:
          gtk_json_printer_start_array (printer, name);
          gtk_json_parser_start_array (parser);
          continue;

        default:
          g_assert_not_reached ();
          return;
        }

      g_free (name);
      gtk_json_parser_next (parser);
    }
}

static gboolean
format (GtkJsonPrinter *printer,
        GFile          *file)
{
  GBytes *bytes;
  GtkJsonParser *parser;
  GError *error = NULL;

  error = NULL;

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
  parse_and_print (parser, printer);

  if (gtk_json_parser_get_error (parser))
    {
      char *uri;
      const GError *parser_error;
      const char *data, *start, *end;
      gsize start_line, start_bytes, start_offset, end_line, end_bytes, end_offset;
      GString *string;

      uri = g_file_get_uri (file);
      parser_error = gtk_json_parser_get_error (parser);
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
                  g_get_prgname (), uri, string->str, parser_error->message);
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
  GtkJsonPrinter *printer;
  Writer writer;
  gboolean res;
  int sv_errno;
  int i;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  
  context = g_option_context_new (NULL);
  /* Translators: this message will appear after the usage string */
  /* and before the list of options.                              */
  g_option_context_set_summary (context, _("Format JSON files."));
  g_option_context_set_description (context, _("json-glib-format formats JSON resources."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_parse (context, &argc, &argv, &error);
  g_option_context_free (context);

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
      return EXIT_FAILURE;
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
      return EXIT_FAILURE;
    }

  if (output == NULL)
    writer_init (&writer, STDOUT_FILENO, FALSE);
  else
    {
      int fd;

      fd = g_open (output, O_CREAT | O_WRONLY, 0666);
      if (fd < 0)
        {
          sv_errno = errno;

          g_printerr (_("%s: %s: error opening file: %s\n"),
                      g_get_prgname (), output, g_strerror (sv_errno));
          res = FALSE;
          return EXIT_FAILURE;
        }
      writer_init (&writer, fd, TRUE);
    }

  printer = gtk_json_printer_new (writer_write,
                                  &writer,
                                  NULL);
  gtk_json_printer_set_flags (printer,
                              (prettify ? GTK_JSON_PRINTER_PRETTY : 0) |
                              (ascii ? GTK_JSON_PRINTER_ASCII : 0));
  gtk_json_printer_set_indentation (printer, indent_spaces);

  res = TRUE;
  i = 0;

  do
    {
      GFile *file = g_file_new_for_commandline_arg (files[i]);

      res = format (printer, file) && res;
      g_object_unref (file);
      writer_write (printer, "\n", &writer);
    }
  while (files[++i] != NULL);

  sv_errno = writer_finish (&writer);
  if (sv_errno)
    {
      g_printerr (_("%s: error writing: %s"), g_get_prgname (), g_strerror (sv_errno));
      res = FALSE;
    }

  gtk_json_printer_free (printer);

  return res ? EXIT_SUCCESS : EXIT_FAILURE;
}
