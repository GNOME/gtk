#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xatom.h>
#endif

static gboolean interactive = FALSE;

static gboolean
stop_main (gpointer data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);

  return G_SOURCE_REMOVE;
}

static void
on_draw (GtkDrawingArea *da,
         cairo_t        *cr,
         int             width,
         int             height,
         gpointer        data)
{
  int i, j;

  for (i = 0; 20 * i < width; i++)
    {
      for (j = 0; 20 * j < height; j++)
        {
          if ((i + j) % 2 == 1)
            cairo_set_source_rgb (cr, 1., 1., 1.);
          else
            cairo_set_source_rgb (cr, 0., 0., 0.);

          cairo_rectangle (cr, 20. * i, 20. *j, 20., 20.);
          cairo_fill (cr);
        }
    }
}

static gboolean
on_keypress (GtkEventControllerKey *key,
             guint keyval,
             guint keycode,
             GdkModifierType state,
             gpointer data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);

  return GDK_EVENT_PROPAGATE;
}

static void
test_default_size (void)
{
  GtkWidget *window;
  GtkWidget *da;
  int w, h;
  gboolean done;

  window = gtk_window_new ();
  if (interactive)
    {
      GtkEventController *controller = gtk_event_controller_key_new ();
      g_signal_connect (controller, "key-pressed", G_CALLBACK (on_keypress), &done);
      gtk_widget_add_controller (window, controller);
    }

  da = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), on_draw, NULL, NULL);
  gtk_window_set_child (GTK_WINDOW (window), da);

  /* check that default size is unset initially */
  gtk_window_get_default_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, -1);
  g_assert_cmpint (h, ==, -1);

  /* check that setting default size before realize works */
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  gtk_window_get_default_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 300);
  g_assert_cmpint (h, ==, 300);

  /* check that the window size is also reported accordingly */
  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 300);
  g_assert_cmpint (h, ==, 300);

  gtk_window_present (GTK_WINDOW (window));

  done = FALSE;
  if (!interactive)
    g_timeout_add (200, stop_main, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  /* check that the window and its content actually gets the right size */
  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 300);
  g_assert_cmpint (h, ==, 300);

  g_assert_cmpint (gtk_widget_get_allocated_width (da), ==, 300);
  g_assert_cmpint (gtk_widget_get_allocated_height (da), ==, 300);

  /* check that setting default size after the fact does not change
   * window size
   */
  gtk_window_set_default_size (GTK_WINDOW (window), 100, 600);
  gtk_window_get_default_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 100);
  g_assert_cmpint (h, ==, 600);

  done = FALSE;
  if (!interactive)
    g_timeout_add (200, stop_main, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 300);
  g_assert_cmpint (h, ==, 300);

  /* check that even hide/show does not pull in the new default */
  gtk_widget_set_visible (window, FALSE);
  gtk_widget_set_visible (window, TRUE);

  done = FALSE;
  if (!interactive)
    g_timeout_add (200, stop_main, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 300);
  g_assert_cmpint (h, ==, 300);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_resize_popup (void)
{
  GtkWidget *window;
  int w, h;
  gboolean done;

  /* testcase for the dnd window */
  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 1, 1);
  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 1);
  g_assert_cmpint (h, ==, 1);

  gtk_window_present (GTK_WINDOW (window));

  done = FALSE;
  if (!interactive)
    g_timeout_add (200, stop_main, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert_cmpint (w, ==, 1);
  g_assert_cmpint (h, ==, 1);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_show_hide (void)
{
  GtkWidget *window;
  int w, h, w1, h1;
  gboolean done;

  /*http://bugzilla.gnome.org/show_bug.cgi?id=696882 */

  /* test that hide/show does not affect the size */

  window = gtk_window_new ();

  gtk_window_present (GTK_WINDOW (window));

  done = FALSE;
  if (!interactive)
    g_timeout_add (200, stop_main, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);

  gtk_widget_hide (window);

  done = FALSE;
  if (!interactive)
    g_timeout_add (200, stop_main, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_get_size (GTK_WINDOW (window), &w1, &h1);
  g_assert_cmpint (w, ==, w1);
  g_assert_cmpint (h, ==, h1);

  gtk_window_present (GTK_WINDOW (window));

  done = FALSE;
  if (!interactive)
    g_timeout_add (200, stop_main, &done);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_get_size (GTK_WINDOW (window), &w1, &h1);
  g_assert_cmpint (w, ==, w1);
  g_assert_cmpint (h, ==, h1);

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  int i;

  gtk_test_init (&argc, &argv);

  for (i = 0; i < argc; i++)
    {
      if (g_strcmp0 (argv[i], "--interactive") == 0)
        interactive = TRUE;
    }

  g_test_add_func ("/window/default-size", test_default_size);
  g_test_add_func ("/window/resize-popup", test_resize_popup);
  g_test_add_func ("/window/show-hide", test_show_hide);

  return g_test_run ();
}
