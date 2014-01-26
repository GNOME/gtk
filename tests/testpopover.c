#include <gtk/gtk.h>

static void
activate (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  g_print ("%s activated\n", g_action_get_name (G_ACTION (action)));
}

static GActionEntry entries[] = {
  { "action1", activate, NULL, NULL, NULL },
  { "action2", NULL, NULL, "true", NULL },
  { "action2a", NULL, NULL, "false", NULL },
  { "action3", NULL, "s", "'three'", NULL },
  { "action4", activate, NULL, NULL, NULL },
  { "action5", activate, NULL, NULL, NULL },
  { "action6", activate, NULL, NULL, NULL },
  { "action7", activate, NULL, NULL, NULL },
  { "action8", activate, NULL, NULL, NULL },
  { "action9", activate, NULL, NULL, NULL },
  { "action10", activate, NULL, NULL, NULL }
};

int main (int argc, char *argv[])
{
  GtkWidget *win;
  GtkWidget *button;
  GtkBuilder *builder;
  GMenuModel *model;
  GtkWidget *popover;
  GSimpleActionGroup *actions;
  GtkWidget *box;

  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);
  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), NULL);

  gtk_widget_insert_action_group (win, "top", G_ACTION_GROUP (actions));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (win), box);

  button = gtk_button_new_with_label ("Pop");
  g_object_set (button, "margin", 10, NULL);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (box), button);

  builder = gtk_builder_new_from_file ("popover.ui");
  model = (GMenuModel *)gtk_builder_get_object (builder, "menu");
  popover = gtk_popover_new_from_model (button, model);
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (gtk_widget_show), popover);

  button = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), model);
  g_object_set (button, "margin", 10, NULL);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_widget_show_all (win);

  gtk_main ();

  return 0;
}
