/* Application Class
 *
 * Demonstrates a simple application.
 *
 * This examples uses GtkApplication, GtkApplicationWindow, GtkBuilder
 * as well as GMenu and GResource. Due to the way GtkApplication is structured,
 * it is run as a separate process.
 */

#include "config.h"

#include <gtk/gtk.h>

#ifdef STANDALONE

static void
show_action_dialog (GSimpleAction *action)
{
  const gchar *name;
  GtkWidget *dialog;

  name = g_action_get_name (G_ACTION (action));

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "You activated action: \"%s\"",
                                    name);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_widget_show (dialog);
}

static void
show_action_infobar (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       window)
{
  GtkWidget *infobar;
  GtkWidget *message;
  gchar *text;
  const gchar *name;
  const gchar *value;

  name = g_action_get_name (G_ACTION (action));
  value = g_variant_get_string (parameter, NULL);

  message = g_object_get_data (G_OBJECT (window), "message");
  infobar = g_object_get_data (G_OBJECT (window), "infobar");
  text = g_strdup_printf ("You activated radio action: \"%s\".\n"
                          "Current value: %s", name, value);
  gtk_label_set_text (GTK_LABEL (message), text);
  gtk_widget_show (infobar);
  g_free (text);
}

static void
activate_action (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        user_data)
{
  show_action_dialog (action);
}

static void
activate_toggle (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GVariant *state;

  show_action_dialog (action);

  state = g_action_get_state (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
  g_variant_unref (state);
}

static void
activate_radio (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  show_action_infobar (action, parameter, user_data);

  g_action_change_state (G_ACTION (action), parameter);
}

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkWidget *window = user_data;

  const gchar *authors[] = {
    "Peter Mattis",
    "Spencer Kimball",
    "Josh MacDonald",
    "and many more...",
    NULL
  };

  const gchar *documentors[] = {
    "Owen Taylor",
    "Tony Gale",
    "Matthias Clasen <mclasen@redhat.com>",
    "and many more...",
    NULL
  };

  gtk_show_about_dialog (GTK_WINDOW (window),
                         "program-name", "GTK+ Code Demos",
                         "version", g_strdup_printf ("%s,\nRunning against GTK+ %d.%d.%d",
                                                     PACKAGE_VERSION,
                                                     gtk_get_major_version (),
                                                     gtk_get_minor_version (),
                                                     gtk_get_micro_version ()),
                         "copyright", "(C) 1997-2013 The GTK+ Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to demonstrate GTK+ functions.",
                         "authors", authors,
                         "documenters", documentors,
                         "logo-icon-name", "gtk3-demo",
                         "title", "About GTK+ Code Demos",
                         NULL);
}

static void
activate_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkApplication *app = user_data;
  GtkWidget *win;
  GList *list, *next;

  list = gtk_application_get_windows (app);
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_widget_destroy (GTK_WIDGET (win));

      list = next;
    }
}

static void
update_statusbar (GtkTextBuffer *buffer,
                  GtkStatusbar  *statusbar)
{
  gchar *msg;
  gint row, col;
  gint count;
  GtkTextIter iter;

  /* clear any previous message, underflow is allowed */
  gtk_statusbar_pop (statusbar, 0);

  count = gtk_text_buffer_get_char_count (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer,
                                    &iter,
                                    gtk_text_buffer_get_insert (buffer));

  row = gtk_text_iter_get_line (&iter);
  col = gtk_text_iter_get_line_offset (&iter);

  msg = g_strdup_printf ("Cursor at row %d column %d - %d chars in document",
                         row, col, count);

  gtk_statusbar_push (statusbar, 0, msg);

  g_free (msg);
}

static void
mark_set_callback (GtkTextBuffer     *buffer,
                   const GtkTextIter *new_location,
                   GtkTextMark       *mark,
                   gpointer           data)
{
  update_statusbar (buffer, GTK_STATUSBAR (data));
}

static void
change_theme_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
  GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);

  g_simple_action_set_state (action, state);
}

static void
change_titlebar_state (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
  GtkWindow *window = user_data;

  gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (window),
                                               g_variant_get_boolean (state));

  g_simple_action_set_state (action, state);
}

static void
change_radio_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
  g_simple_action_set_state (action, state);
}

static GActionEntry app_entries[] = {
  { "new", activate_action, NULL, NULL, NULL },
  { "open", activate_action, NULL, NULL, NULL },
  { "save", activate_action, NULL, NULL, NULL },
  { "save-as", activate_action, NULL, NULL, NULL },
  { "quit", activate_quit, NULL, NULL, NULL },
  { "dark", activate_toggle, NULL, "false", change_theme_state }
};

static GActionEntry win_entries[] = {
  { "titlebar", activate_toggle, NULL, "false", change_titlebar_state },
  { "shape", activate_radio, "s", "'oval'", change_radio_state },
  { "bold", activate_toggle, NULL, "false", NULL },
  { "about", activate_about, NULL, NULL, NULL },
  { "file1", activate_action, NULL, NULL, NULL },
  { "logo", activate_action, NULL, NULL, NULL }
};

static void
clicked_cb (GtkWidget *widget, GtkWidget *info)
{
  gtk_widget_hide (info);
}

static void
startup (GApplication *app)
{
  GtkBuilder *builder;
  GMenuModel *appmenu;
  GMenuModel *menubar;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/application/menus.ui", NULL);

  appmenu = (GMenuModel *)gtk_builder_get_object (builder, "appmenu");
  menubar = (GMenuModel *)gtk_builder_get_object (builder, "menubar");

  gtk_application_set_app_menu (GTK_APPLICATION (app), appmenu);
  gtk_application_set_menubar (GTK_APPLICATION (app), menubar);

  g_object_unref (builder);
}

static void
activate (GApplication *app)
{
  GtkBuilder *builder;
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *contents;
  GtkWidget *status;
  GtkWidget *message;
  GtkWidget *button;
  GtkWidget *infobar;
  GtkWidget *menutool;
  GMenuModel *toolmenu;
  GtkTextBuffer *buffer;

  window = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_window_set_title (GTK_WINDOW (window), "Application Class");
  gtk_window_set_icon_name (GTK_WINDOW (window), "document-open");
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);

  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   window);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/application/application.ui", NULL);

  grid = (GtkWidget *)gtk_builder_get_object (builder, "grid");
  contents = (GtkWidget *)gtk_builder_get_object (builder, "contents");
  status = (GtkWidget *)gtk_builder_get_object (builder, "status");
  message = (GtkWidget *)gtk_builder_get_object (builder, "message");
  button = (GtkWidget *)gtk_builder_get_object (builder, "button");
  infobar = (GtkWidget *)gtk_builder_get_object (builder, "infobar");
  menutool = (GtkWidget *)gtk_builder_get_object (builder, "menutool");
  toolmenu = (GMenuModel *)gtk_builder_get_object (builder, "toolmenu");

  g_object_set_data (G_OBJECT (window), "message", message);
  g_object_set_data (G_OBJECT (window), "infobar", infobar);

  gtk_container_add (GTK_CONTAINER (window), grid);

  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (menutool),
                                 gtk_menu_new_from_model (toolmenu));

  gtk_widget_grab_focus (contents);
  g_signal_connect (button, "clicked", G_CALLBACK (clicked_cb), infobar);

  /* Show text widget info in the statusbar */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contents));
  g_signal_connect_object (buffer, "changed",
                           G_CALLBACK (update_statusbar), status, 0);
  g_signal_connect_object (buffer, "mark-set",
                           G_CALLBACK (mark_set_callback), status, 0);

  update_statusbar (buffer, GTK_STATUSBAR (status));

  gtk_widget_show_all (window);

  g_object_unref (builder);
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;
  GSettings *settings;
  GAction *action;

  gtk_init (NULL, NULL);

  app = gtk_application_new ("org.gtk.Demo2", 0);
  settings = g_settings_new ("org.gtk.Demo");

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);

  action = g_settings_create_action (settings, "color");

  g_action_map_add_action (G_ACTION_MAP (app), action);

  g_signal_connect (app, "startup", G_CALLBACK (startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  g_application_run (G_APPLICATION (app), 0, NULL);

  return 0;
}

#else /* !STANDALONE */

static gboolean name_seen;
static GtkWidget *placeholder;

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  name_seen = TRUE;
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  if (!name_seen)
    return;

  if (placeholder)
    {
      gtk_widget_destroy (placeholder);
      g_object_unref (placeholder);
      placeholder = NULL;
    }
}

#ifdef G_OS_WIN32
#define APP_EXTENSION ".exe"
#else
#define APP_EXTENSION
#endif

GtkWidget *
do_application (GtkWidget *toplevel)
{
  static guint watch = 0;

  if (watch == 0)
    watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                              "org.gtk.Demo2",
                              0,
                              on_name_appeared,
                              on_name_vanished,
                              NULL, NULL);

  if (placeholder == NULL)
    {
      const gchar *command;
      GError *error = NULL;

      if (g_file_test ("./gtk3-demo-application" APP_EXTENSION, G_FILE_TEST_IS_EXECUTABLE))
        command = "./gtk3-demo-application" APP_EXTENSION;
      else
        command = "gtk3-demo-application";

      if (!g_spawn_command_line_async (command, &error))
        {
          g_warning ("%s", error->message);
          g_error_free (error);
        }

      placeholder = gtk_label_new ("");
      g_object_ref_sink (placeholder);
    }
  else
    {
      g_dbus_connection_call_sync (g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL),
                              "org.gtk.Demo2",
                              "/org/gtk/Demo2",
                              "org.gtk.Actions",
                              "Activate",
                              g_variant_new ("(sava{sv})", "quit", NULL, NULL),
                              NULL,
                              0,
                              G_MAXINT,
                              NULL, NULL);
    }

  return placeholder;
}

#endif
