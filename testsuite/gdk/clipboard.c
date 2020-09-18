#include <stdlib.h>

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

static void
value_received (GObject      *source,
                GAsyncResult *res,
                gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  const GValue *value;
  GValue *expected = data;
  GError *error = NULL;

  value = gdk_clipboard_read_value_finish (clipboard, res, &error);

  g_assert_nonnull (value);
  g_assert_no_error (error);

  g_assert_cmpuint (G_VALUE_TYPE (value), ==, G_VALUE_TYPE (expected));
  
  if (G_VALUE_TYPE (value) == G_TYPE_STRING)
    {
      g_assert_cmpstr (g_value_get_string (value), ==, g_value_get_string (expected));
    }
  else
    {
      /* Add a comparison for the missing type here */
      g_assert_cmpuint (G_VALUE_TYPE (value), ==, G_TYPE_INVALID);
    }

  g_value_unset (expected);

  g_main_context_wakeup (NULL);
}

static void
text_received (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
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

static void
remote_clipboard_sync_changed_cb (GdkClipboard *clipboard,
                                  gboolean     *done)
{
  GdkContentFormats *formats = gdk_clipboard_get_formats (clipboard);

  /* X11 needs to query formats first and reports empty until then */
  if (gdk_content_formats_get_gtypes (formats, NULL) == NULL &&
      gdk_content_formats_get_mime_types (formats, NULL) == NULL)
    return;

  *done = TRUE;
}

static void
remote_clipboard_sync (GdkClipboard *clipboard)
{
  guint signal;
  gboolean done;

  done = FALSE;
  signal = g_signal_connect (clipboard, "changed", G_CALLBACK (remote_clipboard_sync_changed_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  g_signal_handler_disconnect (clipboard, signal);
}

static void
test_remote_basic (gconstpointer data)
{
  GdkDisplay *display, *remote_display;
  GdkClipboard *clipboard, *remote_clipboard;
  GdkContentFormats *formats;
  GValue value = G_VALUE_INIT;

  display = gdk_display_get_default ();
  clipboard = gdk_display_get_clipboard (display);
  remote_display = GDK_DISPLAY (data);
  remote_clipboard = gdk_display_get_clipboard (remote_display);

  g_assert_true (gdk_clipboard_get_display (clipboard) == display);
  g_assert_true (gdk_clipboard_get_display (remote_clipboard) == remote_display);

  gdk_clipboard_set_text (clipboard, "testing, 1, 2");
  remote_clipboard_sync (remote_clipboard);

  g_assert_true (gdk_clipboard_is_local (clipboard));
  g_assert_false (gdk_clipboard_is_local (remote_clipboard));
  g_assert_null (gdk_clipboard_get_content (remote_clipboard));

  formats = gdk_clipboard_get_formats (remote_clipboard);

  g_assert_true (gdk_content_formats_contain_gtype (formats, G_TYPE_STRING));
  g_assert_true (gdk_content_formats_contain_mime_type (formats, "text/plain"));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "testing, 1, 2");
  gdk_clipboard_read_value_async (remote_clipboard, G_TYPE_STRING, G_PRIORITY_DEFAULT, NULL, value_received, &value);
  while (G_IS_VALUE (&value))
    g_main_context_iteration (NULL, TRUE);

  g_assert_null (gdk_clipboard_get_content (remote_clipboard));
}

int
main (int argc, char *argv[])
{
  GdkDisplay *remote_display;
  int result;

  g_test_init (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/clipboard/basic", test_clipboard_basic);

  remote_display = gdk_display_open (NULL);

#ifdef GDK_WINDOWING_WAYLAND
  if (!GDK_IS_WAYLAND_DISPLAY (remote_display))
#else
  if (true)
#endif
    {
      g_test_add_data_func ("/clipboard/remote/basic", remote_display, test_remote_basic);
    }

  result = g_test_run ();

  g_object_unref (remote_display);

  return result;
}
