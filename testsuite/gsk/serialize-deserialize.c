#include <gtk/gtk.h>

static void
deserialize_error_func (const GtkCssSection *section,
                        const GError         *error,
                        gpointer              user_data)
{
  char *section_str = gtk_css_section_to_string (section);

  g_error ("Error at %s: %s", section_str, error->message);

  free (section_str);
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  GskRenderNode *node;
  GskRenderNode *deserialized;
  GBytes *bytes;
  GFile *file;

  g_assert (argc == 2);

  gtk_init ();

  file = g_file_new_for_commandline_arg (argv[1]);
  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (bytes != NULL);

  node = gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
  g_assert_no_error (error);

  /* Now serialize */
  g_bytes_unref (bytes);
  bytes = gsk_render_node_serialize (node);
  /* and deserialize again... */
  deserialized = gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
  if (error)
    g_message ("OUTPUT:\n%.*s", (int)g_bytes_get_size (bytes), (char *)g_bytes_get_data (bytes, NULL));

  g_assert_no_error (error);

  /* And check if that all worked. */
  g_assert_cmpint (gsk_render_node_get_node_type (deserialized), ==,
                   gsk_render_node_get_node_type (node));


  g_clear_error (&error);
  g_clear_pointer (&node, gsk_render_node_unref);
  g_clear_pointer (&deserialized, gsk_render_node_unref);
  g_bytes_unref (bytes);
  g_object_unref (file);

  return 0;
}
