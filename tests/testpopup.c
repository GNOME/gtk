#include <gtk/gtk.h>

static void
clicked (GtkButton *button)
{
  g_print ("Yes!\n");
}

static GtkWidget *
add_content (GtkWidget *parent)
{
  GtkWidget *box, *label, *entry, *button;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

  label = gtk_label_new_with_mnemonic ("_Test");
  entry = gtk_entry_new ();
  button = gtk_button_new_with_mnemonic ("_Yes!");
  g_signal_connect (button, "clicked", G_CALLBACK (clicked), NULL);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_set_can_default (button, TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (box), entry);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_container_add (GTK_CONTAINER (parent), box);

  gtk_widget_grab_default (button);

  return box;
}

static gboolean
create_popup (GtkWidget *parent)
{
  GtkWidget *popup;

  popup = gtk_popup_new ();
  gtk_popup_set_relative_to (GTK_POPUP (popup), parent);
  gtk_style_context_add_class (gtk_widget_get_style_context (popup), "background");
  gtk_style_context_add_class (gtk_widget_get_style_context (popup), "frame");

  add_content (popup);

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

  label = add_content (window);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect_swapped (window, "map", G_CALLBACK (create_popup), label);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
