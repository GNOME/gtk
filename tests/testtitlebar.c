#include <gtk/gtk.h>

static void
on_text_changed (GtkEntry       *entry,
                 GParamSpec     *pspec,
                 GtkCssProvider *provider)
{
  const gchar *layout;
  gchar *css;

  layout = gtk_entry_get_text (entry);

  css = g_strdup_printf ("GtkWindow {\n"
                         "  -GtkWindow-decoration-button-layout: '%s';\n"
                         "}", layout);

  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  g_free (css);
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
  GtkCssProvider *provider;
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
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  provider = gtk_css_provider_new ();

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider), 600);

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

  gtk_widget_style_get (window, "decoration-button-layout", &layout, NULL);
  gtk_entry_set_text (GTK_ENTRY (entry), layout);
  g_free (layout);

  g_signal_connect (entry, "notify::text",
                    G_CALLBACK (on_text_changed), provider);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 2, 1, 1);

  label = gtk_label_new ("Close Button");
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
