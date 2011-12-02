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
  GtkTextView *text = g_object_get_data ((GObject*)window, "bloatpad-text");

  gtk_text_buffer_copy_clipboard (gtk_text_view_get_buffer (text),
				  get_clipboard ((GtkWidget*) text));
}

static void
window_paste (GSimpleAction *action,
	      GVariant      *parameter,
	      gpointer       user_data)
{
  GtkWindow *window = GTK_WINDOW (user_data);
  GtkTextView *text = g_object_get_data ((GObject*)window, "bloatpad-text");
  
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
  gtk_window_set_title (GTK_WINDOW (window), "Bloatpad");

  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (scrolled, TRUE);
  gtk_widget_set_vexpand (scrolled, TRUE);
  view = gtk_text_view_new ();

  g_object_set_data ((GObject*)window, "bloatpad-text", view);

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
bloat_pad_activate (GApplication *application)
{
  new_window (application, NULL);
}

static void
bloat_pad_open (GApplication  *application,
                GFile        **files,
                gint           n_files,
                const gchar   *hint)
{
  gint i;

  for (i = 0; i < n_files; i++)
    new_window (application, files[i]);
}

typedef GtkApplication BloatPad;
typedef GtkApplicationClass BloatPadClass;

G_DEFINE_TYPE (BloatPad, bloat_pad, GTK_TYPE_APPLICATION)

static void
bloat_pad_finalize (GObject *object)
{
  G_OBJECT_CLASS (bloat_pad_parent_class)->finalize (object);
}

static void
show_about (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  GtkWindow *window = user_data;

  gtk_show_about_dialog (window,
                         "program-name", "Bloatpad",
                         "title", "About Bloatpad",
                         "comments", "Not much to say, really.",
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

static GActionEntry app_entries[] = {
  { "about", show_about, NULL, NULL, NULL },
  { "quit", quit_app, NULL, NULL, NULL },
};

static GActionGroup *
create_app_actions (void)
{
  GSimpleActionGroup *actions = g_simple_action_group_new ();
  g_simple_action_group_add_entries (actions,
                                     app_entries, G_N_ELEMENTS (app_entries),
                                     NULL);

  return G_ACTION_GROUP (actions);
}

static void
bloat_pad_init (BloatPad *app)
{
  GActionGroup *actions;
  GtkBuilder *builder;

  actions = create_app_actions ();
  g_application_set_action_group (G_APPLICATION (app), actions);
  g_object_unref (actions);

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
                               "<interface>"
                               "  <menu id='app-menu'>"
                               "    <item label='_About Bloatpad' action='app.about'/>"
                               "    <item label='_Quit' action='app.quit'/>"
                               "  </menu>"
                               "  <menu id='menubar'>"
                               "    <submenu label='_Edit'>"
                               "      <item label='_Copy' action='win.copy'/>"
                               "      <item label='_Paste' action='win.paste'/>"
                               "    </submenu>"
                               "    <submenu label='_View'>"
                               "      <item label='_Fullscreen' action='win.fullscreen'/>"
                               "    </submenu>"
                               "  </menu>"
                               "</interface>", -1, NULL);
  g_application_set_app_menu (G_APPLICATION (app), G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu")));
  g_application_set_menubar (G_APPLICATION (app), G_MENU_MODEL (gtk_builder_get_object (builder, "menubar")));
  g_object_unref (builder);
}

static void
bloat_pad_class_init (BloatPadClass *class)
{
  G_OBJECT_CLASS (class)->finalize = bloat_pad_finalize;

  G_APPLICATION_CLASS (class)->activate = bloat_pad_activate;
  G_APPLICATION_CLASS (class)->open = bloat_pad_open;
}

BloatPad *
bloat_pad_new (void)
{
  g_type_init ();

  return g_object_new (bloat_pad_get_type (),
                       "application-id", "org.gtk.Test.bloatpad",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

int
main (int argc, char **argv)
{
  BloatPad *bloat_pad;
  int status;

  bloat_pad = bloat_pad_new ();
  status = g_application_run (G_APPLICATION (bloat_pad), argc, argv);
  g_object_unref (bloat_pad);

  return status;
}
