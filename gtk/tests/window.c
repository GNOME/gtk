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

  gtk_widget_show_all (window);

  g_timeout_add (1000, stop_main, NULL);
  gtk_main ();

  g_assert (gtk_widget_get_allocated_width (window) == 300);
  g_assert (gtk_widget_get_allocated_height (window) == 300);

  g_assert (gtk_widget_get_allocated_width (box) == 300);
  g_assert (gtk_widget_get_allocated_height (box) == 300);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/window/default-size", test_default_size);

  return g_test_run ();
}
