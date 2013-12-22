#include <gtk/gtk.h>

static void
on_text_changed (GtkEntry       *entry,
                 GParamSpec     *pspec,
                 GtkHeaderBar   *bar)
{
  const gchar *layout;

  layout = gtk_entry_get_text (entry);

  gtk_header_bar_set_decoration_layout (bar, layout);
}

static void
activate (GApplication *gapp)
{
  GtkApplication *app = GTK_APPLICATION (gapp);
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *check;
  GtkBuilder *builder;
  GMenuModel *menu;
  gchar *layout;

  g_action_map_add_action (G_ACTION_MAP (gapp), G_ACTION (g_simple_action_new ("test", NULL)));
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
                               "<interface>"
                               "  <menu id='app-menu'>"
                               "    <section>"
                               "      <item>"
                               "        <attribute name='label'>Test item</attribute>"
                               "        <attribute name='action'>app.test</attribute>"
                               "      </item>"
                               "    </section>"
                               "  </menu>"
                               "</interface>", -1, NULL);
  window = gtk_application_window_new (app);
  gtk_window_set_icon_name (GTK_WINDOW (window), "preferences-desktop-font");

  menu = (GMenuModel*)gtk_builder_get_object (builder, "app-menu");
  gtk_application_add_window (app, GTK_WINDOW (window));
  gtk_application_set_app_menu (app, menu);

  header = gtk_header_bar_new ();
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), gtk_button_new_with_label ("Start"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), gtk_button_new_with_label ("End"));
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  grid = gtk_grid_new ();
  g_object_set (grid,
                "halign", GTK_ALIGN_CENTER,
                "margin", 20,
                "row-spacing", 12,
                "column-spacing", 12,
                NULL);

  label = gtk_label_new ("Title");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  g_object_bind_property (header, "title",
                          entry, "text",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

  label = gtk_label_new ("Subtitle");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  g_object_bind_property (header, "subtitle",
                          entry, "text",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);

  label = gtk_label_new ("Layout");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  entry = gtk_entry_new ();

  g_object_get (gtk_widget_get_settings (window), "gtk-decoration-layout", &layout, NULL);
  gtk_entry_set_text (GTK_ENTRY (entry), layout);
  g_free (layout);

  g_signal_connect (entry, "notify::text",
                    G_CALLBACK (on_text_changed), header);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 2, 1, 1);

  label = gtk_label_new ("Decorations");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  check = gtk_check_button_new ();
  g_object_bind_property (header, "show-close-button",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 3, 0, 1, 1);

  label = gtk_label_new ("Has Subtitle");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  check = gtk_check_button_new ();
  g_object_bind_property (header, "has-subtitle",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 2, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 3, 1, 1, 1);

  label = gtk_label_new ("Shell Shows Menu");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  check = gtk_check_button_new ();
  g_object_bind_property (gtk_settings_get_default (), "gtk-shell-shows-app-menu",
                          check, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), label, 2, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check, 3, 2, 1, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Test.titlebar", 0);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
