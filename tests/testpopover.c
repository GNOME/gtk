#include <gtk/gtk.h>

static void
activate (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  g_print ("%s activated\n", g_action_get_name (G_ACTION (action)));
}

static GActionEntry entries[] = {
  { "cut", activate, NULL, NULL, NULL },
  { "copy", activate, NULL, NULL, NULL },
  { "paste", activate, NULL, NULL, NULL },
  { "bold", NULL, NULL, "false", NULL },
  { "italic", NULL, NULL, "false", NULL },
  { "strikethrough", NULL, NULL, "false", NULL },
  { "underline", NULL, NULL, "false", NULL },
  { "set-view", NULL, "s", "'list'", NULL },
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
  GSimpleActionGroup *actions;
  GtkWidget *overlay;
  GtkWidget *grid;
  GtkWidget *popover;
  GtkWidget *label;
  GtkWidget *check;
  GtkWidget *combo;

  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);
  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), NULL);

  gtk_widget_insert_action_group (win, "top", G_ACTION_GROUP (actions));

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (win), overlay);

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_FILL);
  gtk_widget_set_valign (grid, GTK_ALIGN_FILL);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_container_add (GTK_CONTAINER (overlay), grid);

  label = gtk_label_new ("");
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  label = gtk_label_new ("");
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 3, 6, 1, 1);

  builder = gtk_builder_new_from_file ("popover.ui");
  model = (GMenuModel *)gtk_builder_get_object (builder, "menu");

  button = gtk_menu_button_new ();
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), model);
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), TRUE);

  popover = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));

  g_object_set (button, "margin", 10, NULL);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), button);

  label = gtk_label_new ("Popover hexpand");
  check = gtk_check_button_new ();
  g_object_bind_property (popover, "hexpand",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label , 1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 2, 1, 1, 1);

  label = gtk_label_new ("Popover vexpand");
  check = gtk_check_button_new ();
  g_object_bind_property (popover, "vexpand",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label , 1, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 2, 2, 1, 1);

  label = gtk_label_new ("Button direction");
  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "up", "Up");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "down", "Down");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "left", "Left");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "right", "Right");

  g_object_bind_property (button, "direction",
                          combo, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label , 1, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), combo, 2, 3, 1, 1);

  label = gtk_label_new ("Button halign");
  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "fill", "Fill");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "start", "Start");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "end", "End");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "center", "Center");
  g_object_bind_property (button, "halign",
                          combo, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label , 1, 4, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), combo, 2, 4, 1, 1);

  label = gtk_label_new ("Button valign");
  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "fill", "Fill");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "start", "Start");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "end", "End");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "center", "Center");
  g_object_bind_property (button, "valign",
                          combo, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label , 1, 5, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), combo, 2, 5, 1, 1);


  gtk_widget_show_all (win);

  gtk_main ();

  return 0;
}
