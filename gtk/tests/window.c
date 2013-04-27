#include <gtk/gtk.h>

static gboolean
stop_main (gpointer data)
{
  gtk_main_quit ();

  return G_SOURCE_REMOVE;
}

static void
test_default_size (void)
{
  GtkWidget *window;
  GtkWidget *box;
  gint w, h;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  gtk_window_get_default_size (GTK_WINDOW (window), &w, &h);
  g_assert (w == -1 && h == -1);

  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  gtk_window_get_default_size (GTK_WINDOW (window), &w, &h);
  g_assert (w == 300 && h == 300);

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert (w == 300 && h == 300);

  gtk_widget_show_all (window);

  g_timeout_add (1000, stop_main, NULL);
  gtk_main ();

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert (w == 300 && h == 300);

  g_assert (gtk_widget_get_allocated_width (window) == 300);
  g_assert (gtk_widget_get_allocated_height (window) == 300);

  g_assert (gtk_widget_get_allocated_width (box) == 300);
  g_assert (gtk_widget_get_allocated_height (box) == 300);

  gtk_widget_destroy (window);
}

static void
test_resize (void)
{
  GtkWidget *window;
  GtkWidget *box;
  gint w, h;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  gtk_window_resize (GTK_WINDOW (window), 400, 200);

  gtk_widget_show_all (window);

  g_timeout_add (1000, stop_main, NULL);
  gtk_main ();

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert (w == 400 && h == 200);

  gtk_window_resize (GTK_WINDOW (window), 200, 400);

  g_timeout_add (1000, stop_main, NULL);
  gtk_main ();

  gtk_window_get_size (GTK_WINDOW (window), &w, &h);
  g_assert (w == 200 && h == 400);

  gtk_widget_destroy (window);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/window/default-size", test_default_size);
  g_test_add_func ("/window/resize", test_resize);

  return g_test_run ();
}
