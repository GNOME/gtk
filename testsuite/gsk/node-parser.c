#include <gtk/gtk.h>

static void
deserialize_error_func (const GtkCssSection *section,
                        const GError        *error,
                        gpointer             user_data)
{
  char *section_str = gtk_css_section_to_string (section);

  /* We want to parse invalid node files in this test and simply assert that the
   * parser doesn't crash. So, just g_message() here instead of a warning or error. */
  g_message ("Error at %s: %s", section_str, error->message);

  free (section_str);
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  GskRenderNode *node;
  GBytes *bytes;
  GFile *file;

  g_assert (argc == 2);

  gtk_init ();

  file = g_file_new_for_commandline_arg (argv[1]);
  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (bytes != NULL);

  node = gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
  if (error)
    g_test_message ("Error: %s\n", error->message);

  g_clear_error (&error);
  g_clear_pointer (&node, gsk_render_node_unref);
  g_bytes_unref (bytes);
  g_object_unref (file);

  return 0;
}
