#include <gtk/gtk.h>

static int count = 1;

static void
add_child (GtkWidget  *stack,
           const char *title)
{
  char *name;
  GtkWidget *sw, *tv;

  name = g_strdup_printf ("tab%d", count++);

  sw = gtk_scrolled_window_new (NULL, NULL);
  tv = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv)), title, -1);
  g_object_set (tv, "expand", TRUE, NULL);
  gtk_container_add (GTK_CONTAINER (sw), tv);
  gtk_widget_show_all (sw);

  gtk_stack_add_titled (GTK_STACK (stack), sw, name, title);
  gtk_stack_set_visible_child (GTK_STACK (stack), sw);

  g_free (name);
}

static void
add_stack_child (GtkWidget *stack)
{
  char *title;

  title = g_strdup_printf ("Page %d", count);
  add_child (stack, title);
  g_free (title);
}

static void
populate (GtkWidget *stack)
{
  add_child (stack, "Enigma machine - Wikipedia, the free ecyclopedia");
  add_child (stack, "Ada Lovelace");
  add_child (stack, "Analytical Engine | computer | Britannica.com");
  add_child (stack, "Charles Babbage");
  add_child (stack, "Turing Machine — from Wolfram MathWorld");
  add_child (stack, "Enigma machine - Wikipedia, the free ecyclopedia");
  add_child (stack, "Enigma machine - Wikipedia, the free ecyclopedia");
  add_child (stack, "Enigma machine - Wikipedia, the free ecyclopedia");
  add_child (stack, "Enigma machine - Wikipedia, the free ecyclopedia");
}

static void
attention (GtkButton *button, GtkStack *stack)
{
  GList *children;
  children = gtk_container_get_children (GTK_CONTAINER (stack));
  gtk_container_child_set (GTK_CONTAINER (stack), children->data,
                           "needs-attention", TRUE,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (stack), g_list_last (children)->data,
                           "needs-attention", TRUE,
                           NULL);
  g_list_free (children);
}

static const char css[] =
  "tab.needs-attention { border-bottom: solid 4px red; }"
  "scrolledwindow undershoot.left.needs-attention { background: none; border-left: solid 4px red; }"
  "scrolledwindow undershoot.right.needs-attention { background: none; border-right: solid 4px red; }";

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *titlebar;
  GtkWidget *vbox;
  GtkWidget *tabs;
  GtkWidget *stack;
  GtkWidget *button;
  GtkWidget *hbox;
  GtkCssProvider *provider;

  gtk_init (NULL, NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), provider, 800);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 800);
  titlebar = gtk_header_bar_new ();
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (titlebar), TRUE);
  gtk_widget_show (titlebar);
  gtk_window_set_titlebar (GTK_WINDOW (window), titlebar);
  gtk_window_set_title (GTK_WINDOW (window), "Tab Strip Test");

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  stack = gtk_stack_new ();
  g_object_set (stack, "expand", TRUE, NULL);

  tabs = gtk_tab_strip_new ();
  gtk_tab_strip_set_closable (GTK_TAB_STRIP (tabs), TRUE);
  gtk_tab_strip_set_stack (GTK_TAB_STRIP (tabs), GTK_STACK (stack));

  button = gtk_button_new_from_icon_name ("tab-new-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (titlebar), button);
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (add_stack_child), stack);

  gtk_box_pack_start (GTK_BOX (vbox), tabs, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), stack, TRUE, TRUE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new_with_label ("Attention");
  g_signal_connect (button, "clicked", G_CALLBACK (attention), stack);
  gtk_container_add (GTK_CONTAINER (hbox), button);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  populate  (stack);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
