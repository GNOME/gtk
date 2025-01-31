#include "config.h"

#include <gtk/gtk.h>

typedef GtkApplication DemoApplication;
typedef GtkApplicationClass DemoApplicationClass;

static GType demo_application_get_type (void);
G_DEFINE_TYPE (DemoApplication, demo_application, GTK_TYPE_APPLICATION)

typedef struct {
  GtkApplicationWindow parent_instance;

  GtkWidget *message;
  GtkWidget *infobar;
  GtkWidget *status;
  GtkWidget *menubutton;
  GMenuModel *toolmenu;
  GtkTextBuffer *buffer;
} DemoApplicationWindow;
typedef GtkApplicationWindowClass DemoApplicationWindowClass;

static GType demo_application_window_get_type (void);
G_DEFINE_TYPE (DemoApplicationWindow, demo_application_window, GTK_TYPE_APPLICATION_WINDOW)

static void create_window (GApplication *app, const char *contents);

static void
show_action_dialog (GSimpleAction *action)
{
  GtkAlertDialog *dialog;

  dialog = gtk_alert_dialog_new ("You activated action: \"%s\"",
                                 g_action_get_name (G_ACTION (action)));
  gtk_alert_dialog_show (dialog, NULL);
  g_object_unref (dialog);
}

static void
show_action_infobar (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data)
{
  DemoApplicationWindow *window = data;
  char *text;
  const char *name;
  const char *value;

  name = g_action_get_name (G_ACTION (action));
  value = g_variant_get_string (parameter, NULL);

  text = g_strdup_printf ("You activated radio action: \"%s\".\n"
                          "Current value: %s", name, value);
  gtk_label_set_text (GTK_LABEL (window->message), text);
  gtk_widget_set_visible (window->infobar, TRUE);
  g_free (text);
}

static void
activate_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  show_action_dialog (action);
}

static void
activate_new (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  GApplication *app = user_data;

  create_window (app, NULL);
}

static void
open_response_cb (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GApplication *app = G_APPLICATION (user_data);
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_open_finish (dialog, result, &error);
  if (file)
    {
      char *contents;

      if (g_file_load_contents (file, NULL, &contents, NULL, NULL, &error))
        {
          create_window (app, contents);
          g_free (contents);
        }
    }

  if (error)
    {
      GtkAlertDialog *alert;

      alert = gtk_alert_dialog_new ("Error loading file: \"%s\"", error->message);
      gtk_alert_dialog_show (alert, NULL);
      g_object_unref (alert);
      g_error_free (error);
    }

  g_object_unref (app);
}


static void
activate_open (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GApplication *app = user_data;
  GtkFileDialog *dialog;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_open (dialog, NULL, NULL, open_response_cb, g_object_ref (app));
  g_object_unref (dialog);
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

  const char *authors[] = {
    "Peter Mattis",
    "Spencer Kimball",
    "Josh MacDonald",
    "and many more...",
    NULL
  };

  const char *documentors[] = {
    "Owen Taylor",
    "Tony Gale",
    "Matthias Clasen <mclasen@redhat.com>",
    "and many more...",
    NULL
  };

  gtk_show_about_dialog (GTK_WINDOW (window),
                         "program-name", "GTK Code Demos",
                         "version", g_strdup_printf ("%s,\nRunning against GTK %d.%d.%d",
                                                     PACKAGE_VERSION,
                                                     gtk_get_major_version (),
                                                     gtk_get_minor_version (),
                                                     gtk_get_micro_version ()),
                         "copyright", "(C) 1997-2013 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to demonstrate GTK functions.",
                         "authors", authors,
                         "documenters", documentors,
                         "logo-icon-name", "org.gtk.Demo4",
                         "title", "About GTK Code Demos",
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

      gtk_window_destroy (GTK_WINDOW (win));

      list = next;
    }
}

static void
delete_messages (gpointer data)
{
  g_list_free_full ((GList *)data, g_free);
}

static void
pop_message (GtkWidget *status)
{
  GList *messages = (GList *) g_object_steal_data (G_OBJECT (status), "messages");

  if (messages)
    {
      char *message = messages->data;
      messages = g_list_remove (messages, message);

      g_object_set_data_full (G_OBJECT (status), "messages",
                              messages, delete_messages);

      gtk_label_set_label (GTK_LABEL (status), message);
    }
}

static void
push_message (GtkWidget  *status,
              const char *message)
{
  GList *messages = (GList *) g_object_steal_data (G_OBJECT (status), "messages");

  gtk_label_set_label (GTK_LABEL (status), message);
  messages = g_list_prepend (messages, g_strdup (message));
  g_object_set_data_full (G_OBJECT (status), "messages",
                          messages, delete_messages);
}

static void
update_statusbar (GtkTextBuffer         *buffer,
                  DemoApplicationWindow *window)
{
  char *msg;
  int row, col;
  int count;
  GtkTextIter iter;

  /* clear any previous message, underflow is allowed */
  pop_message (window->status);

  count = gtk_text_buffer_get_char_count (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer,
                                    &iter,
                                    gtk_text_buffer_get_insert (buffer));

  row = gtk_text_iter_get_line (&iter);
  col = gtk_text_iter_get_line_offset (&iter);

  msg = g_strdup_printf ("Cursor at row %d column %d - %d chars in document",
                         row, col, count);

  push_message (window->status, msg);

  g_free (msg);
}

static void
mark_set_callback (GtkTextBuffer         *buffer,
                   const GtkTextIter     *new_location,
                   GtkTextMark           *mark,
                   DemoApplicationWindow *window)
{
  update_statusbar (buffer, window);
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
change_radio_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
  g_simple_action_set_state (action, state);
}

static GActionEntry app_entries[] = {
  { "new", activate_new, NULL, NULL, NULL },
  { "open", activate_open, NULL, NULL, NULL },
  { "save", activate_action, NULL, NULL, NULL },
  { "save-as", activate_action, NULL, NULL, NULL },
  { "quit", activate_quit, NULL, NULL, NULL },
  { "dark", activate_toggle, NULL, "false", change_theme_state }
};

static GActionEntry win_entries[] = {
  { "shape", activate_radio, "s", "'oval'", change_radio_state },
  { "bold", activate_toggle, NULL, "false", NULL },
  { "about", activate_about, NULL, NULL, NULL },
  { "file1", activate_action, NULL, NULL, NULL },
  { "logo", activate_action, NULL, NULL, NULL }
};

static void
clicked_cb (GtkWidget *widget, DemoApplicationWindow *window)
{
  gtk_widget_set_visible (window->infobar, FALSE);
}

static void
startup (GApplication *app)
{
  GtkBuilder *builder;
  const char *session_id;

  G_APPLICATION_CLASS (demo_application_parent_class)->startup (app);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/application_demo/menus.ui", NULL);

  gtk_application_set_menubar (GTK_APPLICATION (app),
                               G_MENU_MODEL (gtk_builder_get_object (builder, "menubar")));

  g_object_unref (builder);

  session_id = gtk_application_get_current_session_id (GTK_APPLICATION (app));

  if (session_id)
    {
      GSettings *settings = g_settings_new ("org.gtk.Demo4.Application");

      g_settings_set_string (settings, "session-id", session_id);

      g_object_unref (settings);
    }
}

static void
create_window (GApplication *app,
               const char   *content)
{
  DemoApplicationWindow *window;

  window = (DemoApplicationWindow *)g_object_new (demo_application_window_get_type (),
                                                  "application", app,
                                                  "show-menubar", TRUE,
                                                  NULL);
  if (content)
    gtk_text_buffer_set_text (window->buffer, content, -1);

  gtk_window_present (GTK_WINDOW (window));
}

static void
activate (GApplication *app)
{
  create_window (app, NULL);
}

static void
demo_application_init (DemoApplication *app)
{
  GSettings *settings;
  GAction *action;

  settings = g_settings_new ("org.gtk.Demo4.Application");

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);

  action = g_settings_create_action (settings, "color");

  g_action_map_add_action (G_ACTION_MAP (app), action);

  g_object_unref (settings);
}

static void
demo_application_class_init (DemoApplicationClass *class)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (class);

  app_class->startup = startup;
  app_class->activate = activate;
}

static void
demo_application_window_init (DemoApplicationWindow *window)
{
  GtkWidget *popover;

  gtk_widget_init_template (GTK_WIDGET (window));

  popover = gtk_popover_menu_new_from_model (window->toolmenu);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (window->menubutton), popover);

  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   window);
}

static void
demo_application_window_dispose (GObject *object)
{
  DemoApplicationWindow *window = (DemoApplicationWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (window), demo_application_window_get_type ());

  G_OBJECT_CLASS (demo_application_window_parent_class)->dispose (object);
}

static void
demo_application_window_class_init (DemoApplicationWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_application_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/application_demo/application.ui");
  gtk_widget_class_bind_template_child (widget_class, DemoApplicationWindow, message);
  gtk_widget_class_bind_template_child (widget_class, DemoApplicationWindow, infobar);
  gtk_widget_class_bind_template_child (widget_class, DemoApplicationWindow, status);
  gtk_widget_class_bind_template_child (widget_class, DemoApplicationWindow, buffer);
  gtk_widget_class_bind_template_child (widget_class, DemoApplicationWindow, menubutton);
  gtk_widget_class_bind_template_child (widget_class, DemoApplicationWindow, toolmenu);
  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_statusbar);
  gtk_widget_class_bind_template_callback (widget_class, mark_set_callback);
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;
  GSettings *settings;
  char *session_id;

  app = GTK_APPLICATION (g_object_new (demo_application_get_type (),
                                       "application-id", "org.gtk.Demo4.App",
                                       "flags", G_APPLICATION_HANDLES_OPEN,
                                       NULL));

  settings = g_settings_new ("org.gtk.Demo4.Application");
  session_id = g_settings_get_string (settings, "session-id");
  gtk_application_set_session_id (app, session_id);
  g_free (session_id);
  g_object_unref (settings);

  return g_application_run (G_APPLICATION (app), 0, NULL);
}
