#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_center_box_new ();
  gtk_container_add (GTK_CONTAINER (window), box);

  gtk_center_box_set_start_widget (GTK_CENTER_BOX (box), gtk_label_new ("Start Start Start Start Start"));
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (box), gtk_label_new ("Center"));
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (box), gtk_label_new ("End"));

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
