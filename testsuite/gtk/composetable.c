#include <gtk/gtk.h>
#include <locale.h>

#include "../gtk/gtkcomposetable.h"
#include "../gtk/gtkimcontextsimpleseqs.h"
#include "testsuite/testutils.h"

static void
append_escaped (GString    *str,
                const char *s)
{
  for (const char *p = s; *p; p = g_utf8_next_char (p))
    {
      gunichar ch = g_utf8_get_char (p);
      if (ch == '"')
        g_string_append (str, "\\\"");
      else if (ch == '\\')
        g_string_append (str, "\\\\");
      else if (g_unichar_isprint (ch))
        g_string_append_unichar (str, ch);
      else
        {
          guint n[8] = { 0, };
          int i = 0;
          while (ch != 0)
            {
              n[i++] = ch & 7;
              ch = ch >> 3;
            }
          for (; i >= 0; i--)
            g_string_append_printf (str, "\\%o", n[i]);
        }
    }
}

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
      char buf[8] = { 0 };

      for (j = 0; j < table->max_seq_len; j++)
        g_string_append_printf (str, "<U%x> ", seq[j]);

      value = (seq[table->max_seq_len] << 16) | seq[table->max_seq_len + 1];
      if ((value & (1 << 31)) != 0)
        {
          const char *out = &table->char_data[value & ~(1 << 31)];

          g_string_append (str, ": \"");
          append_escaped (str, out);
          g_string_append (str, "\"\n");
        }
      else
        {
          g_unichar_to_utf8 (value, buf);
          g_string_append_printf (str, ": \"%s\" # U%x\n", buf, value);
        }

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

/* Check matching against a small table */
static void
compose_table_match (void)
{
  GSList *tables = NULL;
  GtkComposeTable *table;
  char *file;
  guint16 buffer[8] = { 0, };
  gboolean finish, match, ret;
  GString *output;

  output = g_string_new ("");

  file = g_build_filename (g_test_get_dir (G_TEST_DIST), "compose", "match", NULL);

  tables = gtk_compose_table_list_add_file (tables, file);

  g_assert_true (g_slist_length (tables) == 1);

  table = tables->data;

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = 0;
  ret = gtk_compose_table_check (table, buffer, 1, &finish, &match, output);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (output->len == 0);

  buffer[0] = GDK_KEY_a;
  buffer[1] = 0;
  ret = gtk_compose_table_check (table, buffer, 1, &finish, &match, output);
  g_assert_false (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (output->len == 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_s;
  buffer[2] = GDK_KEY_e;
  ret = gtk_compose_table_check (table, buffer, 3, &finish, &match, output);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (output->len == 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_s;
  buffer[2] = GDK_KEY_e;
  buffer[3] = GDK_KEY_q;
  ret = gtk_compose_table_check (table, buffer, 4, &finish, &match, output);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_true (match);
  g_assert_cmpstr (output->str, ==, "!");

  g_string_set_size (output, 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_s;
  buffer[2] = GDK_KEY_e;
  buffer[3] = GDK_KEY_q;
  buffer[4] = GDK_KEY_u;
  ret = gtk_compose_table_check (table, buffer, 5, &finish, &match, output);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_cmpstr (output->str, ==, "?");

  g_string_free (output, TRUE);
  g_free (file);
}

/* just check some random sequences */
static void
compose_table_match_compact (void)
{
  const GtkComposeTableCompact table = {
    gtk_compose_seqs_compact,
    5,
    30,
    6
  };
  guint16 buffer[8] = { 0, };
  gboolean finish, match, ret;
  gunichar ch;

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = 0;

  ret = gtk_compose_table_compact_check (&table, buffer, 1, &finish, &match, &ch);
  g_assert_true (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (ch == 0);

  buffer[0] = GDK_KEY_a;
  buffer[1] = GDK_KEY_b;
  buffer[2] = GDK_KEY_c;
  buffer[3] = 0;

  ret = gtk_compose_table_compact_check (&table, buffer, 3, &finish, &match, &ch);
  g_assert_false (ret);
  g_assert_false (finish);
  g_assert_false (match);
  g_assert_true (ch == 0);

  buffer[0] = GDK_KEY_Multi_key;
  buffer[1] = GDK_KEY_parenleft;
  buffer[2] = GDK_KEY_j;
  buffer[3] = GDK_KEY_parenright;
  buffer[4] = 0;

  ret = gtk_compose_table_compact_check (&table, buffer, 4, &finish, &match, &ch);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_true (ch == 0x24d9); /* CIRCLED LATIN SMALL LETTER J */

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_space;
  buffer[2] = 0;

  ret = gtk_compose_table_compact_check (&table, buffer, 2, &finish, &match, &ch);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_true (ch == 0x27);

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_dead_acute;
  buffer[2] = 0;

  ret = gtk_compose_table_compact_check (&table, buffer, 2, &finish, &match, &ch);
  g_assert_true (ret);
  g_assert_true (finish);
  g_assert_true (match);
  g_assert_true (ch == 0xb4);
}

static void
match_algorithmic (void)
{
  guint16 buffer[8] = { 0, };
  gboolean ret;
  gunichar ch;

  buffer[0] = GDK_KEY_a;
  buffer[1] = GDK_KEY_b;

  ret = gtk_check_algorithmically (buffer, 2, &ch);
  g_assert_false (ret);
  g_assert_true (ch == 0);

  buffer[0] = GDK_KEY_dead_abovering;
  buffer[1] = GDK_KEY_A;

  ret = gtk_check_algorithmically (buffer, 2, &ch);
  g_assert_true (ret);
  g_assert_true (ch == 0xc5);

  buffer[0] = GDK_KEY_A;
  buffer[1] = GDK_KEY_dead_abovering;

  ret = gtk_check_algorithmically (buffer, 2, &ch);
  g_assert_false (ret);
  g_assert_true (ch == 0);

  buffer[0] = GDK_KEY_dead_dasia;
  buffer[1] = GDK_KEY_dead_perispomeni;
  buffer[2] = GDK_KEY_Greek_alpha;

  ret = gtk_check_algorithmically (buffer, 3, &ch);
  g_assert_true (ret);
  g_assert_true (ch == 0x1f07);

  buffer[0] = GDK_KEY_dead_perispomeni;
  buffer[1] = GDK_KEY_dead_dasia;
  buffer[2] = GDK_KEY_Greek_alpha;

  ret = gtk_check_algorithmically (buffer, 3, &ch);
  g_assert_true (ret);
  g_assert_true (ch == 0x1f07);

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_dead_cedilla;
  buffer[2] = GDK_KEY_c;

  ret = gtk_check_algorithmically (buffer, 2, &ch);
  g_assert_true (ret);
  g_assert_cmphex (ch, ==, 0);

  ret = gtk_check_algorithmically (buffer, 3, &ch);
  g_assert_true (ret);
  g_assert_cmphex (ch, ==, 0x1e09);

  buffer[0] = GDK_KEY_dead_cedilla;
  buffer[1] = GDK_KEY_dead_acute;
  buffer[2] = GDK_KEY_c;

  ret = gtk_check_algorithmically (buffer, 3, &ch);
  g_assert_true (ret);
  g_assert_cmphex (ch, ==, 0x1e09);

  ret = gtk_check_algorithmically (buffer, 2, &ch);
  g_assert_true (ret);

  buffer[0] = GDK_KEY_dead_acute;
  buffer[1] = GDK_KEY_dead_cedilla;
  buffer[2] = GDK_KEY_dead_grave;

  ret = gtk_check_algorithmically (buffer, 3, &ch);
  g_assert_false (ret);

  buffer[0] = GDK_KEY_dead_diaeresis;
  buffer[1] = GDK_KEY_a;

  ret = gtk_check_algorithmically (buffer, 2, &ch);
  g_assert_true (ret);
  g_assert_cmphex (ch, ==, 0xe4);
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
  g_test_add_data_func ("/compose-table/hex", "hex", compose_table_compare);
  g_test_add_data_func ("/compose-table/codepoint", "codepoint", compose_table_compare);
  g_test_add_data_func ("/compose-table/multi", "multi", compose_table_compare);
  g_test_add_data_func ("/compose-table/strings", "strings", compose_table_compare);
  g_test_add_func ("/compose-table/match", compose_table_match);
  g_test_add_func ("/compose-table/match-compact", compose_table_match_compact);
  g_test_add_func ("/compose-table/match-algorithmic", match_algorithmic);

  return g_test_run ();
}
