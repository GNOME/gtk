#include <gtk/gtk.h>

static void
test_window_focus (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *entry1;
  GtkWidget *entry2;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("label1"));
  entry1 = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (box), entry1);
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("label2"));
  entry2 = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (box), entry2);

  gtk_widget_show_all (box);

  g_assert_null (gtk_window_get_focus (GTK_WINDOW (window)));

  gtk_window_set_focus (GTK_WINDOW (window), entry1);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry1);

  gtk_widget_show_now (window);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry1);

  gtk_widget_grab_focus (entry2);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry2);

  gtk_widget_hide (window);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry2);

  gtk_window_set_focus (GTK_WINDOW (window), entry1);

  g_assert (gtk_window_get_focus (GTK_WINDOW (window)) == entry1);

  gtk_widget_destroy (window);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/focus/window", test_window_focus);

  return g_test_run ();
}
