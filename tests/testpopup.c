#include <gtk/gtk.h>

static gboolean
create_popup (GtkWidget *parent,
              GtkWidget *label)
{
  GtkWidget *popup, *box;

  popup = gtk_popup_new ();
  gtk_popup_set_relative_to (GTK_POPUP (popup), label);
  gtk_style_context_add_class (gtk_widget_get_style_context (popup), "background");
  gtk_style_context_add_class (gtk_widget_get_style_context (popup), "frame");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("Test"));
  gtk_container_add (GTK_CONTAINER (box), gtk_entry_new ());
  gtk_container_add (GTK_CONTAINER (popup), box);

  gtk_widget_show (popup);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *label;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);

  label = gtk_entry_new ();
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (window), label);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (window, "map", G_CALLBACK (create_popup), label);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
