#include <gtk/gtk.h>

static char *cursor_names[] = {
  "none",
  "default",
  "help",
  "pointer",
  "context-menu",
  "progress",
  "wait",
  "cell",
  "crosshair",
  "text",
  "vertical-text",
  "alias",
  "copy",
  "no-drop",
  "move",
  "not-allowed",
  "grab",
  "grabbing",
  "all-scroll",
  "col-resize",
  "row-resize",
  "n-resize",
  "e-resize",
  "s-resize",
  "w-resize",
  "ne-resize",
  "nw-resize",
  "sw-resize",
  "se-resize",
  "ew-resize",
  "ns-resize",
  "nesw-resize",
  "nwse-resize",
  "zoom-in",
  "zoom-out",
  "dnd-ask",
};

static void
test_cursor_existence (gconstpointer name)
{
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gdk_display_get_default ();
  cursor = gdk_cursor_new_from_name (display, name);
  g_assert (cursor != NULL);
  g_object_unref (cursor);
}

static void
test_cursor_nonexistence_subprocess (void)
{
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gdk_display_get_default ();
  cursor = gdk_cursor_new_from_name (display, "non-existing-cursor");
  g_assert (cursor == NULL);
}

static void
test_cursor_nonexistence (void)
{
  g_test_trap_subprocess ("/non-existing-cursors/subprocess/non-existing-cursor", 0, 0);
  g_test_trap_assert_passed ();
}

int
main (int argc, char *argv[])
{
  guint i;
  char *test_name;

  g_test_init (&argc, &argv, NULL);
  gtk_init ();

  for (i = 0; i < G_N_ELEMENTS (cursor_names); i++)
    {
      test_name = g_strdup_printf ("/standard-cursor-names/%s", cursor_names[i]);
      g_test_add_data_func (test_name, cursor_names[i], test_cursor_existence);
      g_free (test_name);
    }

  g_test_add_func ("/non-existing-cursors/subprocess/non-existing-cursor", test_cursor_nonexistence_subprocess);
  g_test_add_func ("/non-existing-cursors/non-existing-cursor", test_cursor_nonexistence);

  return g_test_run();
}
