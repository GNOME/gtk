#include <gtk/gtk.h>
#include "../testutils.h"

static const char *file;

static void
test_serialize_roundtrip (void)
{
  char *data;
  size_t size;
  GError *error = NULL;
  GBytes *bytes, *bytes1, *bytes2;
  GskRenderNode *node1, *node2;
  char *diff;

  g_file_get_contents (file, &data, &size, &error);
  g_assert_no_error (error);
  bytes = g_bytes_new_take (data, size);

  node1 = gsk_render_node_deserialize (bytes, NULL, NULL);
  bytes1 = gsk_render_node_serialize (node1);
  node2 = gsk_render_node_deserialize (bytes1, NULL, NULL);
  bytes2 = gsk_render_node_serialize (node2);

  diff = diff_bytes (file, bytes1, bytes2);

  if (diff && diff[0])
    {
      g_test_message ("%s serialize roundtrip fail:\n%s", file, diff);
      g_test_fail ();
    }

  g_free (diff);
  gsk_render_node_unref (node1);
  gsk_render_node_unref (node2);
  g_bytes_unref (bytes);
  g_bytes_unref (bytes1);
  g_bytes_unref (bytes2);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  file = argv[1];
  g_test_add_func ("/node/serialize/roundtrip", test_serialize_roundtrip);

  return g_test_run ();
}
