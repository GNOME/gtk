#include <gtk/gtk.h>

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *frame;
  GtkWidget *box3;
  GtkWidget *view;
  GtkWidget *button;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (box), frame);
  view = gtk_text_view_new ();
  gtk_widget_set_vexpand (view, TRUE);
  gtk_container_add (GTK_CONTAINER (box), view);
  box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_start (box3, 10);
  gtk_widget_set_margin_end (box3, 10);
  gtk_widget_set_margin_top (box3, 10);
  gtk_widget_set_margin_bottom (box3, 10);

  gtk_widget_add_css_class (box3, GTK_STYLE_CLASS_LINKED);
  button = gtk_button_new_from_icon_name ("document-new-symbolic");
  gtk_container_add (GTK_CONTAINER (box3), button);
  button = gtk_button_new_from_icon_name ("document-open-symbolic");
  gtk_container_add (GTK_CONTAINER (box3), button);
  button = gtk_button_new_from_icon_name ("document-save-symbolic");
  gtk_container_add (GTK_CONTAINER (box3), button);

  gtk_frame_set_child (GTK_FRAME (frame), box3);
  
  gtk_widget_show (GTK_WIDGET (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
