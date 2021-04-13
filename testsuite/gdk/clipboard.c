#include <stdlib.h>

#include <gtk/gtk.h>

static void
text_received (GObject *source,
               GAsyncResult *res,
               gpointer data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  gboolean *done = data;
  GError *error = NULL;
  char *text;

  text = gdk_clipboard_read_text_finish (clipboard, res, &error);

  g_assert_no_error (error);
  g_assert_cmpstr (text, ==, "testing, 1, 2");

  g_free (text);

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
test_clipboard_basic (void)
{
  GdkDisplay *display;
  GdkClipboard *clipboard;
  GdkContentFormats *formats;
  GdkContentProvider *content;
  gboolean done;
  gboolean ret;
  GValue value = G_VALUE_INIT;
  GError *error = NULL;

  display = gdk_display_get_default ();
  clipboard = gdk_display_get_clipboard (display);

  g_assert_true (gdk_clipboard_get_display (clipboard) == display);

  gdk_clipboard_set_text (clipboard, "testing, 1, 2");

  g_assert_true (gdk_clipboard_is_local (clipboard));

  formats = gdk_clipboard_get_formats (clipboard);

  g_assert_true (gdk_content_formats_contain_gtype (formats, G_TYPE_STRING));
  g_assert_true (gdk_content_formats_contain_mime_type (formats, "text/plain"));

  done = FALSE;

  gdk_clipboard_read_text_async (clipboard, NULL, text_received, &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  content = gdk_clipboard_get_content (clipboard);
  g_assert_nonnull (content);

  g_value_init (&value, G_TYPE_STRING);
  ret = gdk_content_provider_get_value (content, &value, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_assert_true (G_VALUE_TYPE (&value) == G_TYPE_STRING);
  g_assert_cmpstr (g_value_get_string (&value), ==, "testing, 1, 2");

  g_value_unset (&value);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/clipboard/basic", test_clipboard_basic);

  return g_test_run ();
}
