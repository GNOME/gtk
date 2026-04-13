#include <gtk/gtk.h>

#include "gtk/svg/gtksvgprivate.h"
#include "gtk/svg/gtksvgelementprivate.h"

static void
test_tree_traverse (void)
{
  char *file;
  GtkSvg *svg;
  char *contents;
  size_t length;
  GError *error = NULL;
  GBytes *bytes;
  GString *s;

  file = g_test_build_filename (G_TEST_DIST, "traverse.svg", NULL);
  g_file_get_contents (file, &contents, &length, &error);
  g_assert_no_error (error);

  bytes = g_bytes_new_take (contents, length);
  svg = gtk_svg_new_from_bytes (bytes);
  g_assert_nonnull (svg);

  gtk_svg_set_load_time (svg, 0);

  s = g_string_new ("");

  while (gtk_svg_move_focus (svg, GTK_DIR_TAB_FORWARD))
    {
      g_string_append_printf (s, "%s", svg_element_get_id (svg->focus));
    }

  g_assert_cmpstr (s->str, ==, "xABCDEFGHI");

  g_assert_null (svg->focus);
  gtk_svg_grab_focus (svg);
  g_assert_nonnull (svg->focus);
  gtk_svg_lose_focus (svg);
  g_assert_null (svg->focus);

  g_string_set_size (s, 0);

  while (gtk_svg_move_focus (svg, GTK_DIR_TAB_BACKWARD))
    {
      g_string_append_printf (s, "%s", svg_element_get_id (svg->focus));
    }

  g_assert_cmpstr (s->str, ==, "IHGFEDCBAx");

  g_string_free (s, TRUE);
  g_object_unref (svg);
  g_bytes_unref (bytes);
  g_free (file);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/svg/tree-traverse", test_tree_traverse);

  return g_test_run ();
}

/* vim:set foldmethod=marker: */
