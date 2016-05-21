#include <gtk/gtk.h>

static void
add_stack_child (GtkWidget *stack)
{
  static int count;
  char *name;
  char *title;
  GtkWidget *sw, *tv;

  count++;

  name = g_strdup_printf ("tab%d", count);
  title = g_strdup_printf ("Page %d", count);

  sw = gtk_scrolled_window_new (NULL, NULL);
  tv = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv)), title, -1);
  g_object_set (tv, "expand", TRUE, NULL);
  gtk_container_add (GTK_CONTAINER (sw), tv);
  gtk_widget_show_all (sw);

  gtk_stack_add_titled (GTK_STACK (stack), sw, name, title);

  g_free (name);
  g_free (title);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *tabs;
  GtkWidget *box;
  GtkWidget *stack;
  GtkWidget *button;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  stack = gtk_stack_new ();
  g_object_set (stack, "expand", TRUE, NULL);

  tabs = gtk_tab_strip_new ();
  gtk_tab_strip_set_closable (GTK_TAB_STRIP (tabs), TRUE);
  gtk_tab_strip_set_scrollable (GTK_TAB_STRIP (tabs), TRUE);
  gtk_tab_strip_set_stack (GTK_TAB_STRIP (tabs), GTK_STACK (stack));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (box), tabs, TRUE, TRUE, 0);

  button = gtk_button_new_from_icon_name ("tab-new-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (add_stack_child), stack);

  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), stack, TRUE, TRUE, 0);

  add_stack_child (stack);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
