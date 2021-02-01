#include <gtk/gtk.h>
#include <locale.h>

#include "../gtk/gtkcomposetable.h"
#include "testsuite/testutils.h"

static char *
gtk_compose_table_print (GtkComposeTable *table)
{
  int i, j;
  guint16 *seq;
  GString *str;

  str = g_string_new ("");

  g_string_append_printf (str, "# n_seqs: %d\n# max_seq_len: %d\n",
                          table->n_seqs,
                          table->max_seq_len);

  for (i = 0, seq = table->data; i < table->n_seqs; i++, seq += table->max_seq_len + 2)
    {
      gunichar value;
      char buf[7] = { 0 };

      for (j = 0; j < table->max_seq_len; j++)
        g_string_append_printf (str, "<U%x> ", seq[j]);

      value = 0x10000 * seq[table->max_seq_len] + seq[table->max_seq_len + 1];
      g_unichar_to_utf8 (value, buf);

      g_string_append_printf (str, ": \"%s\" # U%x\n", buf, value);
    }

  return g_string_free (str, FALSE);
}

static void
generate_output (const char *file)
{
  GSList *tables = NULL;
  GtkComposeTable *table;
  char *output;

  tables = gtk_compose_table_list_add_file (tables, file);
  table = tables->data;
  output = gtk_compose_table_print (table);

  g_print ("%s", output);
}

static void
compose_table_compare (gconstpointer data)
{
  const char *basename = data;
  GSList *tables = NULL;
  GtkComposeTable *table;
  char *file;
  char *expected;
  char *output;
  char *diff;
  GError *error = NULL;

  file = g_build_filename (g_test_get_dir (G_TEST_DIST), "compose", basename, NULL);
  expected = g_strconcat (file, ".expected", NULL);

  tables = gtk_compose_table_list_add_file (tables, file);

  g_assert_true (g_slist_length (tables) == 1);

  table = tables->data;

  output = gtk_compose_table_print (table);
  diff = diff_with_file (expected, output, -1, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_print ("Resulting output doesn't match reference:\n%s", diff);
      g_test_fail ();
    }

  g_free (output);
  g_free (file);
  g_free (expected);
}

int
main (int argc, char *argv[])
{
  char *dir;

  dir = g_dir_make_tmp ("composetableXXXXXX", NULL);
  g_setenv ("XDG_CACHE_HOME", dir, TRUE);
  g_free (dir);

  if (argc == 3 && strcmp (argv[1], "--generate") == 0)
    {
      setlocale (LC_ALL, "");

      generate_output (argv[2]);
      return 0;
    }

  gtk_test_init (&argc, &argv, NULL);

  g_test_add_data_func ("/compose-table/basic", "basic", compose_table_compare);
  g_test_add_data_func ("/compose-table/long", "long", compose_table_compare);
  g_test_add_data_func ("/compose-table/octal", "octal", compose_table_compare);
  g_test_add_data_func ("/compose-table/codepoint", "codepoint", compose_table_compare);
  g_test_add_data_func ("/compose-table/multi", "multi", compose_table_compare);

  return g_test_run ();
}
