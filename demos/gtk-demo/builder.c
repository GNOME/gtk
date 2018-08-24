/* Builder
 *
 * Demonstrates an interface loaded from a XML description.
 */

#include <gtk/gtk.h>

static void
quit_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkWidget *window = user_data;

  gtk_widget_destroy (window);
}

static void
about_activate (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkBuilder *builder;
  GtkWidget *about_dlg;

  builder = g_object_get_data (G_OBJECT (window), "builder");
  about_dlg = GTK_WIDGET (gtk_builder_get_object (builder, "aboutdialog1"));
  gtk_dialog_run (GTK_DIALOG (about_dlg));
  gtk_widget_hide (about_dlg);
}

static void
menu_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  g_print ("%s action not available\n", g_action_get_name (G_ACTION (action)));
}

static GActionEntry win_entries[] = {
  { "quit", quit_activate, NULL, NULL, NULL },
  { "about", about_activate, NULL, NULL, NULL },
  { "help", menu_activate, NULL, NULL, NULL }
};

static GActionEntry menu_entries[] = {
  { "new", menu_activate, NULL, NULL, NULL },
  { "open", menu_activate, NULL, NULL, NULL },
  { "save", menu_activate, NULL, NULL, NULL },
  { "save_as", menu_activate, NULL, NULL, NULL },
  { "copy", menu_activate, NULL, NULL, NULL },
  { "cut", menu_activate, NULL, NULL, NULL },
  { "paste", menu_activate, NULL, NULL, NULL },
};

GtkWidget *
do_builder (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *toolbar;
  GActionGroup *actions;

  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/builder/demo.ui");

      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      toolbar = GTK_WIDGET (gtk_builder_get_object (builder, "toolbar1"));
      gtk_style_context_add_class (gtk_widget_get_style_context (toolbar),
                                   "primary-toolbar");
      actions = (GActionGroup*)g_simple_action_group_new ();
      g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                       win_entries, G_N_ELEMENTS (win_entries),
                                       window);
      gtk_widget_insert_action_group (window, "win", actions);
      actions = (GActionGroup*)g_simple_action_group_new ();
      g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                       menu_entries, G_N_ELEMENTS (menu_entries),
                                       window);
      gtk_widget_insert_action_group (window, "menu", actions);

      g_object_set_data_full (G_OBJECT(window), "builder", builder, g_object_unref);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
