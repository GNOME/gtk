#include <stdlib.h>

#include <gdk/gdk.h>

static void
count_changed (GdkClipboard *clipboard, gpointer data)
{
  gint *count = data;

  *count += 1;
}

typedef struct {
  gboolean called;
  gchar *text;
  GdkPixbuf *pixbuf;
  GBytes *bytes;
  GError *error;
  GMainLoop *loop;
} GetData;

static void
get_text (GObject *source, GAsyncResult *res, gpointer data)
{
  GetData *gd = data;

  gd->text = gdk_clipboard_get_text_finish (GDK_CLIPBOARD (source), res, &gd->error);
  gd->called = TRUE;
}

static void
get_image (GObject *source, GAsyncResult *res, gpointer data)
{
  GetData *gd = data;

  gd->pixbuf = gdk_clipboard_get_image_finish (GDK_CLIPBOARD (source), res, &gd->error);
  gd->called = TRUE;
}

static void
get_bytes (GObject *source, GAsyncResult *res, gpointer data)
{
  GetData *gd = data;

  gd->bytes = gdk_clipboard_get_bytes_finish (GDK_CLIPBOARD (source), res, &gd->error);
  gd->called = TRUE;
}

static gboolean
wait_get (gpointer data)
{
  GetData *gd = data;

  if (gd->called)
    {
      g_main_loop_quit (gd->loop);
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
test_clipboard_text (void)
{
  GdkClipboard *clipboard;
  const gchar **content_types;
  gint count;
  GetData *gd;

  count = 0;

  clipboard = gdk_clipboard_fallback_new ();
  g_signal_connect (clipboard, "changed", G_CALLBACK (count_changed), &count);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));

  content_types = gdk_clipboard_get_content_types (clipboard);
  g_assert (content_types == NULL);

  gdk_clipboard_set_text (clipboard, "ABC");
  
  g_assert (gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));
  g_assert_cmpint (count, ==, 1);

  gd = g_new0 (GetData, 1);
  gdk_clipboard_get_text_async (clipboard, NULL, get_text, gd);

  gd->loop = g_main_loop_new (NULL, FALSE);
  g_idle_add (wait_get, gd);
  g_main_loop_run (gd->loop);

  g_assert_cmpstr (gd->text, ==, "ABC");
  g_assert_no_error (gd->error);

  g_free (gd->text);
  g_main_loop_unref (gd->loop);
  g_free (gd);

  gdk_clipboard_clear (clipboard);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));
  g_assert_cmpint (count, ==, 2);
 
  g_object_unref (clipboard);
}

static void
test_clipboard_image (void)
{
  GdkClipboard *clipboard;
  const gchar **content_types;
  gint count;
  GdkPixbuf *pixbuf;
  GetData *gd;

  count = 0;

  clipboard = gdk_clipboard_fallback_new ();
  g_signal_connect (clipboard, "changed", G_CALLBACK (count_changed), &count);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));

  content_types = gdk_clipboard_get_content_types (clipboard);
  g_assert (content_types == NULL);

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 16, 16);
  gdk_clipboard_set_image (clipboard, pixbuf);
  
  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));
  g_assert_cmpint (count, ==, 1);

  g_assert_cmpint (G_OBJECT (pixbuf)->ref_count, >=, 2);

  gd = g_new0 (GetData, 1);
  gdk_clipboard_get_image_async (clipboard, NULL, get_image, gd);

  gd->loop = g_main_loop_new (NULL, FALSE);
  g_idle_add (wait_get, gd);
  g_main_loop_run (gd->loop);

  g_assert (gd->pixbuf == pixbuf);
  g_assert_no_error (gd->error);

  g_assert_cmpint (G_OBJECT (pixbuf)->ref_count, >=, 3);

  g_object_unref (gd->pixbuf);

  g_main_loop_unref (gd->loop);
  g_free (gd);

  gdk_clipboard_clear (clipboard);

  g_assert_cmpint (G_OBJECT (pixbuf)->ref_count, ==, 1);
  g_object_unref (pixbuf);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));
  g_assert_cmpint (count, ==, 2);
 
  g_object_unref (clipboard);
}

static void
test_clipboard_bytes (void)
{
  GdkClipboard *clipboard;
  const gchar **content_types;
  gint count;
  GBytes *bytes;
  GetData *gd;

  count = 0;

  clipboard = gdk_clipboard_fallback_new ();
  g_signal_connect (clipboard, "changed", G_CALLBACK (count_changed), &count);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));

  content_types = gdk_clipboard_get_content_types (clipboard);
  g_assert (content_types == NULL);

  bytes = g_bytes_new_static ("012346789", 10);
  gdk_clipboard_set_bytes (clipboard, bytes, "application/octet-stream");
  
  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (gdk_clipboard_data_available (clipboard, "application/octet-stream"));
  g_assert_cmpint (count, ==, 1);

  gd = g_new0 (GetData, 1);
  gdk_clipboard_get_bytes_async (clipboard, "application/octet-stream", NULL, get_bytes, gd);

  gd->loop = g_main_loop_new (NULL, FALSE);
  g_idle_add (wait_get, gd);
  g_main_loop_run (gd->loop);

  g_assert (g_bytes_equal (gd->bytes, bytes));
  g_assert_no_error (gd->error);

  g_bytes_unref (gd->bytes);

  g_main_loop_unref (gd->loop);
  g_free (gd);

  gdk_clipboard_clear (clipboard);

  g_bytes_unref (bytes);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));
  g_assert_cmpint (count, ==, 2);
 
  g_object_unref (clipboard);
}

static gboolean freed;

static void
free_data (gpointer data)
{
  freed = TRUE;
  g_free (data);
}

static void
provider (GdkClipboard  *clipboard,
          const gchar   *content_type,
          GOutputStream *output,
          gpointer       user_data)
{
}

static void
test_clipboard_data (void)
{
  GdkClipboard *clipboard;
  gint count;
  const gchar *content_types[3] = {
    "application/octet-stream",
    "application/pdf",
    NULL
  };

  count = 0;

  clipboard = gdk_clipboard_fallback_new ();
  g_signal_connect (clipboard, "changed", G_CALLBACK (count_changed), &count);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (!gdk_clipboard_data_available (clipboard, "application/octet-stream"));

  gdk_clipboard_set_data (clipboard, content_types,
                          provider, g_strdup ("0123456789"), free_data);

  g_assert (!gdk_clipboard_text_available (clipboard));
  g_assert (!gdk_clipboard_image_available (clipboard));
  g_assert (gdk_clipboard_data_available (clipboard, "application/octet-stream"));
  g_assert (gdk_clipboard_data_available (clipboard, "application/pdf"));
  g_assert (!gdk_clipboard_data_available (clipboard, "yellow submarine"));
  g_assert_cmpint (count, ==, 1);

  g_object_unref (clipboard);

  g_assert (freed);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gdk_init (NULL, NULL);

  g_test_add_func ("/clipboard/text", test_clipboard_text);
  g_test_add_func ("/clipboard/image", test_clipboard_image);
  g_test_add_func ("/clipboard/bytes", test_clipboard_bytes);
  g_test_add_func ("/clipboard/data", test_clipboard_data);

  return g_test_run ();
}
