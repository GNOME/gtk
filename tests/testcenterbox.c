#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkLabel *child;

  gtk_init ();

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_center_box_new ();
  gtk_container_add (GTK_CONTAINER (window), box);

  child = gtk_label_new ("Start Start Start Start Start");
  gtk_label_set_ellipsize (child, PANGO_ELLIPSIZE_END);
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (box), GTK_WIDGET (child));

  child = gtk_label_new ("Center");
  gtk_label_set_ellipsize (child, PANGO_ELLIPSIZE_END);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (box), GTK_WIDGET (child));

  child = gtk_label_new ("End");
  gtk_label_set_ellipsize (child, PANGO_ELLIPSIZE_END);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (box), GTK_WIDGET (child));

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
