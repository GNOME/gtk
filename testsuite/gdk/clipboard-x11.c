#include <gtk/gtk.h>

#include <gdk/x11/gdkx.h>

typedef struct
{
  gboolean done;
  char *text;
  GError *error;
} Request;

static guint
count_xevent_handlers (GdkDisplay *display)
{
  guint signal_id;
  guint count;

  signal_id = g_signal_lookup ("xevent", G_OBJECT_TYPE (display));
  g_assert_cmpuint (signal_id, !=, 0);

  count = g_signal_handlers_block_matched (display,
                                           G_SIGNAL_MATCH_ID,
                                           signal_id,
                                           0,
                                           NULL,
                                           NULL,
                                           NULL);
  g_signal_handlers_unblock_matched (display,
                                     G_SIGNAL_MATCH_ID,
                                     signal_id,
                                     0,
                                     NULL,
                                     NULL,
                                     NULL);

  return count;
}

static void
read_text_done (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  Request *request = user_data;

  request->text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (source),
                                                   result,
                                                   &request->error);
  request->done = TRUE;
}

static void
run_request (void)
{
  Request request = { 0, };
  GdkDisplay *display;
  GdkClipboard *clipboard;

  display = gdk_display_open (NULL);
  g_assert_nonnull (display);
  g_assert_true (display != gdk_display_get_default ());

  clipboard = gdk_display_get_clipboard (display);
  gdk_clipboard_read_text_async (clipboard, NULL, read_text_done, &request);

  while (!request.done)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (request.error);
  g_assert_cmpstr (request.text, ==, "small clipboard payload");

  g_free (request.text);
  gdk_display_close (display);
}

static void
test_output_stream_is_finalized (void)
{
  GdkDisplay *display;
  GdkClipboard *clipboard;
  guint handlers_before;
  guint handlers_after;
  guint i;

  display = gdk_display_get_default ();
  if (!GDK_IS_X11_DISPLAY (display))
    {
      g_test_skip ("The test requires the X11 backend");
      return;
    }

  clipboard = gdk_display_get_clipboard (display);
  gdk_clipboard_set_text (clipboard, "small clipboard payload");

  /* Complete any startup work before taking the baseline. */
  run_request ();
  handlers_before = count_xevent_handlers (display);

  /* Run more requests than there are existing handlers. On the broken
   * implementation each request leaks at least one handler, so the final count
   * exceeds the baseline even if every pre-existing handler disappears.
   */
  for (i = 0; i <= handlers_before; i++)
    run_request ();

  handlers_after = count_xevent_handlers (display);
  g_assert_cmpuint (handlers_after, <=, handlers_before);
}

int
main (int argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/clipboard/x11/output-stream-is-finalized",
                   test_output_stream_is_finalized);

  return g_test_run ();
}
