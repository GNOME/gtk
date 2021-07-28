#include <gtk/gtk.h>
#include "gtk/gtkcomposetable.h"
#include <locale.h>
#include <stdlib.h>

/* This program reads a Compose file and generates files with sequences,
 * character data, and definitions for the builtin compose table of GTK.
 * Run it like this:
 *
 *   compose-parse Compose sequences chars gtkcomposedata.h
 *
 * The GTK build expects the output files to be in the source tree, in
 * the gtk/compose directory.
 */
int
main (int argc, char *argv[])
{
  GtkComposeTable *table;
  GError *error = NULL;
  GString *str;

  setlocale (LC_ALL, "");

  if (argc < 5)
    {
      g_print ("Usage: compose-parse INPUT OUTPUT1 OUTPUT2 OUTPUT3\n");
      exit (1);
    }

  table = gtk_compose_table_parse (argv[1], NULL);
  if (!table)
    g_error ("Failed to parse %s", argv[1]);

  /* data_size is the size in guint16 */
  if (!g_file_set_contents (argv[2], (char *)table->data, 2 * table->data_size, &error))
    g_error ("%s", error->message);

  if (!g_file_set_contents (argv[3], table->char_data, table->n_chars + 1, &error))
    g_error ("%s", error->message);

  str = g_string_new ("");
  g_string_append (str,
                   "#ifndef __GTK_COMPOSE_DATA__\n"
                   "#define __GTK_COMPOSE_DATA__\n"
                   "\n");
  g_string_append_printf (str,
                   "#define MAX_SEQ_LEN %d\n", table->max_seq_len);
  g_string_append_printf (str,
                   "#define N_INDEX_SIZE %d\n", table->n_index_size);
  g_string_append_printf (str,
                   "#define DATA_SIZE %d\n", table->data_size);
  g_string_append_printf (str,
                   "#define N_CHARS %d\n", table->n_chars);
  g_string_append (str,
                   "\n"
                   "#endif\n");

  if (!g_file_set_contents (argv[4], str->str, str->len, &error))
    g_error ("%s", error->message);

  g_string_free (str, TRUE);

  return 0;
}
