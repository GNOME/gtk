#include <stdlib.h>
#include <gtk/gtk.h>

static void
activate_toggle (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GVariant *state;

  state = g_action_get_state (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
  g_variant_unref (state);
}

static void
change_fullscreen_state (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
  if (g_variant_get_boolean (state))
    gtk_window_fullscreen (user_data);
  else
    gtk_window_unfullscreen (user_data);

  g_simple_action_set_state (action, state);
}

static GtkClipboard *
get_clipboard (GtkWidget *widget)
{
  return gtk_widget_get_clipboard (widget, gdk_atom_intern_static_string ("CLIPBOARD"));
}

static void
window_copy (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);
  GtkTextView *text = g_object_get_data ((GObject*)window, "plugman-text");

  gtk_text_buffer_copy_clipboard (gtk_text_view_get_buffer (text),
                                  get_clipboard ((GtkWidget*) text));
}

static void
window_paste (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);
  GtkTextView *text = g_object_get_data ((GObject*)window, "plugman-text");
  
  gtk_text_buffer_paste_clipboard (gtk_text_view_get_buffer (text),
                                   get_clipboard ((GtkWidget*) text),
                                   NULL,
                                   TRUE);

}

static GActionEntry win_entries[] = {
  { "copy", window_copy, NULL, NULL, NULL },
  { "paste", window_paste, NULL, NULL, NULL },
  { "fullscreen", activate_toggle, NULL, "false", change_fullscreen_state }
};

static void
new_window (GApplication *app,
            GFile        *file)
{
  GtkWidget *window, *grid, *scrolled, *view;

  window = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_window_set_default_size ((GtkWindow*)window, 640, 480);
  g_action_map_add_action_entries (G_ACTION_MAP (window), win_entries, G_N_ELEMENTS (win_entries), window);
  gtk_window_set_title (GTK_WINDOW (window), "Plugman");

  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (scrolled, TRUE);
  gtk_widget_set_vexpand (scrolled, TRUE);
  view = gtk_text_view_new ();

  g_object_set_data ((GObject*)window, "plugman-text", view);

  gtk_container_add (GTK_CONTAINER (scrolled), view);

  gtk_grid_attach (GTK_GRID (grid), scrolled, 0, 0, 1, 1);

  if (file != NULL)
    {
      gchar *contents;
      gsize length;

      if (g_file_load_contents (file, NULL, &contents, &length, NULL, NULL))
        {
          GtkTextBuffer *buffer;

          buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
          gtk_text_buffer_set_text (buffer, contents, length);
          g_free (contents);
        }
    }

  gtk_widget_show_all (GTK_WIDGET (window));
}

static void
plug_man_activate (GApplication *application)
{
  new_window (application, NULL);
}

static void
plug_man_open (GApplication  *application,
                GFile        **files,
                gint           n_files,
                const gchar   *hint)
{
  gint i;

  for (i = 0; i < n_files; i++)
    new_window (application, files[i]);
}

typedef GtkApplication PlugMan;
typedef GtkApplicationClass PlugManClass;

G_DEFINE_TYPE (PlugMan, plug_man, GTK_TYPE_APPLICATION)

static void
plug_man_finalize (GObject *object)
{
  G_OBJECT_CLASS (plug_man_parent_class)->finalize (object);
}

static void
show_about (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  gtk_show_about_dialog (NULL,
                         "program-name", "Plugman",
                         "title", "About Plugman",
                         "comments", "A cheap Bloatpad clone.",
                         NULL);
}


static void
quit_app (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  GList *list, *next;
  GtkWindow *win;

  g_print ("Going down...\n");

  list = gtk_application_get_windows (GTK_APPLICATION (g_application_get_default ()));
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_widget_destroy (GTK_WIDGET (win));

      list = next;
    }
}

static gboolean is_red_plugin_enabled;
static gboolean is_black_plugin_enabled;

static gboolean
plugin_enabled (const gchar *name)
{
  if (g_strcmp0 (name, "red") == 0)
    return is_red_plugin_enabled;
  else
    return is_black_plugin_enabled;
}

static GMenuModel *
find_plugin_menu (void)
{
  return (GMenuModel*) g_object_get_data (G_OBJECT (g_application_get_default ()), "plugin-menu");
}

static void
plugin_action (GAction  *action,
               GVariant *parameter,
               gpointer  data)
{
  GApplication *app;
  GList *list;
  GtkWindow *window;
  GtkWidget *text;
  GdkRGBA color;

  app = g_application_get_default ();
  list = gtk_application_get_windows (GTK_APPLICATION (app));
  window = GTK_WINDOW (list->data);
  text = g_object_get_data ((GObject*)window, "plugman-text");

  gdk_rgba_parse (&color, g_action_get_name (action));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_widget_override_color (text, 0, &color);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
enable_plugin (const gchar *name)
{
  GMenuModel *plugin_menu;
  GAction *action;

  g_print ("Enabling '%s' plugin\n", name);

  action = (GAction *)g_simple_action_new (name, NULL);
  g_signal_connect (action, "activate", G_CALLBACK (plugin_action), (gpointer)name);
  g_action_map_add_action (G_ACTION_MAP (g_application_get_default ()), action);
  g_print ("Actions of '%s' plugin added\n", name);
  g_object_unref (action);

  plugin_menu = find_plugin_menu ();
  if (plugin_menu)
    {
      GMenu *section;
      GMenuItem *item;
      gchar *label;
      gchar *action_name;

      section = g_menu_new ();
      label = g_strdup_printf ("Turn text %s", name);
      action_name = g_strconcat ("app.", name, NULL);
      g_menu_insert (section, 0, label, action_name);
      g_free (label);
      g_free (action_name);
      item = g_menu_item_new_section (NULL, (GMenuModel*)section);
      g_menu_item_set_attribute (item, "id", "s", name);
      g_menu_append_item (G_MENU (plugin_menu), item);
      g_object_unref (item);
      g_object_unref (section);
      g_print ("Menus of '%s' plugin added\n", name);
    }
  else
    g_warning ("Plugin menu not found\n");

  if (g_strcmp0 (name, "red") == 0)
    is_red_plugin_enabled = TRUE;
  else
    is_black_plugin_enabled = TRUE;
}

static void
disable_plugin (const gchar *name)
{
  GMenuModel *plugin_menu;

  g_print ("Disabling '%s' plugin\n", name);

  plugin_menu = find_plugin_menu ();
  if (plugin_menu)
    {
      const gchar *id;
      gint i;

      for (i = 0; i < g_menu_model_get_n_items (plugin_menu); i++)
        {
           if (g_menu_model_get_item_attribute (plugin_menu, i, "id", "s", &id) &&
               g_strcmp0 (id, name) == 0)
             {
               g_menu_remove (G_MENU (plugin_menu), i);
               g_print ("Menus of '%s' plugin removed\n", name);
             }
        }
    }
  else
    g_warning ("Plugin menu not found\n");

  g_action_map_remove_action (G_ACTION_MAP (g_application_get_default ()), name);
  g_print ("Actions of '%s' plugin removed\n", name);

  if (g_strcmp0 (name, "red") == 0)
    is_red_plugin_enabled = FALSE;
  else
    is_black_plugin_enabled = FALSE;
}

static void
enable_or_disable_plugin (GtkToggleButton *button,
                          const gchar     *name)
{
  if (plugin_enabled (name))
    disable_plugin (name);
  else
    enable_plugin (name);
}


static void
configure_plugins (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GtkBuilder *builder;
  GtkWidget *dialog;
  GtkWidget *check;
  GError *error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
                               "<interface>"
                               "  <object class='GtkDialog' id='plugin-dialog'>"
                               "    <property name='border-width'>12</property>"
                               "    <property name='title'>Plugins</property>"
                               "    <child internal-child='vbox'>"
                               "      <object class='GtkBox' id='content-area'>"
                               "        <property name='visible'>True</property>"
                               "        <child>"
                               "          <object class='GtkCheckButton' id='red-plugin'>"
                               "            <property name='label' translatable='yes'>Red Plugin - turn your text red</property>"
                               "            <property name='visible'>True</property>"
                               "          </object>"
                               "        </child>"
                               "        <child>"
                               "          <object class='GtkCheckButton' id='black-plugin'>"
                               "            <property name='label' translatable='yes'>Black Plugin - turn your text black</property>"
                               "            <property name='visible'>True</property>"
                               "          </object>"
                               "        </child>"
                               "      </object>"
                               "    </child>"
                               "    <child internal-child='action_area'>"
                               "      <object class='GtkButtonBox' id='action-area'>"
                               "        <property name='visible'>True</property>"
                               "        <child>"
                               "          <object class='GtkButton' id='close-button'>"
                               "            <property name='label' translatable='yes'>Close</property>"
                               "            <property name='visible'>True</property>"
                               "          </object>"
                               "        </child>"
                               "      </object>"
                               "    </child>"
                               "    <action-widgets>"
                               "      <action-widget response='-5'>close-button</action-widget>"
                               "    </action-widgets>"
                               "  </object>"
                               "</interface>", -1, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  dialog = (GtkWidget *)gtk_builder_get_object (builder, "plugin-dialog");
  check = (GtkWidget *)gtk_builder_get_object (builder, "red-plugin");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), plugin_enabled ("red"));
  g_signal_connect (check, "toggled", G_CALLBACK (enable_or_disable_plugin), "red");
  check = (GtkWidget *)gtk_builder_get_object (builder, "black-plugin");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), plugin_enabled ("black"));
  g_signal_connect (check, "toggled", G_CALLBACK (enable_or_disable_plugin), "black");

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static GActionEntry app_entries[] = {
  { "about", show_about, NULL, NULL, NULL },
  { "quit", quit_app, NULL, NULL, NULL },
  { "plugins", configure_plugins, NULL, NULL, NULL },
};

static void
plug_man_startup (GApplication *application)
{
  GtkBuilder *builder;

  G_APPLICATION_CLASS (plug_man_parent_class)
    ->startup (application);

  g_action_map_add_action_entries (G_ACTION_MAP (application), app_entries, G_N_ELEMENTS (app_entries), application);

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
                               "<interface>"
                               "  <menu id='app-menu'>"
                               "    <section>"
                               "      <item>"
                               "        <attribute name='label' translatable='yes'>_About Plugman</attribute>"
                               "        <attribute name='action'>app.about</attribute>"
                               "      </item>"
                               "    </section>"
                               "    <section>"
                               "      <item>"
                               "        <attribute name='label' translatable='yes'>_Quit</attribute>"
                               "        <attribute name='action'>app.quit</attribute>"
                               "        <attribute name='accel'>&lt;Primary&gt;q</attribute>"
                               "      </item>"
                               "    </section>"
                               "  </menu>"
                               "  <menu id='menubar'>"
                               "    <submenu>"
                               "      <attribute name='label' translatable='yes'>_Edit</attribute>"
                               "      <section>"
                               "        <item>"
                               "          <attribute name='label' translatable='yes'>_Copy</attribute>"
                               "          <attribute name='action'>win.copy</attribute>"
                               "        </item>"
                               "        <item>"
                               "          <attribute name='label' translatable='yes'>_Paste</attribute>"
                               "          <attribute name='action'>win.paste</attribute>"
                               "        </item>"
                               "      </section>"
                               "      <item><link name='section' id='plugins'>"
                               "      </link></item>"
                               "      <section>"
                               "        <item>"
                               "          <attribute name='label' translatable='yes'>Plugins</attribute>"
                               "          <attribute name='action'>app.plugins</attribute>"
                               "        </item>"
                               "      </section>"
                               "    </submenu>"
                               "    <submenu>"
                               "      <attribute name='label' translatable='yes'>_View</attribute>"
                               "      <section>"
                               "        <item>"
                               "          <attribute name='label' translatable='yes'>_Fullscreen</attribute>"
                               "          <attribute name='action'>win.fullscreen</attribute>"
                               "        </item>"
                               "      </section>"
                               "    </submenu>"
                               "  </menu>"
                               "</interface>", -1, NULL);
  gtk_application_set_app_menu (GTK_APPLICATION (application), G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu")));
  gtk_application_set_menubar (GTK_APPLICATION (application), G_MENU_MODEL (gtk_builder_get_object (builder, "menubar")));
  g_object_set_data_full (G_OBJECT (application), "plugin-menu", gtk_builder_get_object (builder, "plugins"), g_object_unref);
  g_object_unref (builder);
}

static void
plug_man_init (PlugMan *app)
{
}

static void
plug_man_class_init (PlugManClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  application_class->startup = plug_man_startup;
  application_class->activate = plug_man_activate;
  application_class->open = plug_man_open;

  object_class->finalize = plug_man_finalize;

}

PlugMan *
plug_man_new (void)
{
  return g_object_new (plug_man_get_type (),
                       "application-id", "org.gtk.Test.plugman",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

int
main (int argc, char **argv)
{
  PlugMan *plug_man;
  int status;
  const gchar *accels[] = { "F11", NULL };

  plug_man = plug_man_new ();
  gtk_application_set_accels_for_action (GTK_APPLICATION (plug_man),
                                         "win.fullscreen", accels);
  status = g_application_run (G_APPLICATION (plug_man), argc, argv);
  g_object_unref (plug_man);

  return status;
}
